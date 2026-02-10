#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define CHANNELS 1


//Modificado por: Paula Carreño 202320149 y Juan Andrés Moreno 202321829

#pragma pack(push, 1)
typedef struct {
    char           riff[4];        /* "RIFF" */
    unsigned long  chunkSize;      /* 36 + dataSize */
    char           wave[4];        /* "WAVE" */
    char           fmt[4];         /* "fmt " */
    unsigned long  subchunk1Size;  /* 16 */
    unsigned short audioFormat;    /* 1 = PCM */
    unsigned short numChannels;    /* 1 = mono */
    unsigned long  sampleRate;
    unsigned long  byteRate;
    unsigned short blockAlign;
    unsigned short bitsPerSample;  /* 16 */
    char           data[4];        /* "data" */
    unsigned long  dataSize;       /* bytes de audio */
} WavHeader;
#pragma pack(pop)

/*
ATENCIÓN: la función initSineTable está comentada. 
Solo se incluye para presentar las operaciones usadas para calcular los valores de la tabla correspondiente. 

static void initSineTable(void) {
    int i;
    for (i = 0; i < TABLE_SIZE; i++) {
        double angle = (2.0 * 3.141592653589793 * (double)i) / (double)TABLE_SIZE;
        int v = (int)(32767.0 * sin(angle));   // Q15 
        sineTable[i] = (short)v;
    }
}
*/
/* Tabla seno Q15 (256 entradas): -32767..32767 */
/* La tabla es una vuelta completa del seno, muestreada en 256 puntos equiespaciados en fase.*/
/* Cada entrada reprsenta seno(2*pi*i/256)
/* El índice NO es tiempo, NO es frecuencia, NO es amplitud.*/

static short sineTable[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24278, 24811, 25329, 25831, 26318, 26789, 27244, 27683, 28105, 28510, 28898, 29269, 29622, 29957,
    30274, 30572, 30852, 31113, 31356, 31580, 31785, 31970, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31970, 31785, 31580, 31356, 31113, 30852, 30572,
    30274, 29957, 29622, 29269, 28898, 28510, 28105, 27683, 27244, 26789, 26318, 25831, 25329, 24811, 24278, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24278, -24811, -25329, -25831, -26318, -26789, -27244, -27683, -28105, -28510, -28898, -29269, -29622, -29957,
    -30274, -30572, -30852, -31113, -31356, -31580, -31785, -31970, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31970, -31785, -31580, -31356, -31113, -30852, -30572,
    -30274, -29957, -29622, -29269, -28898, -28510, -28105, -27683, -27244, -26789, -26318, -25831, -25329, -24811, -24278, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804
};

/*
 * -----------------------------------------------------------------------------
 *  freq_to_phaseStep
 * -----------------------------------------------------------------------------
 *  Calcula el incremento de fase necesario para generar un tono de una
 *  frecuencia dada, a partir de la frecuencia de muestreo del sistema.
 *
 *  A partir de la frecuencia deseada en Hertz (freqHz), esta función devuelve
 *  el valor que debe sumarse al acumulador de fase en cada muestra para avanzar sobre el total de muestreos a hacer
 *
 * -----------------------------------------------------------------------------
 */
unsigned int freq_to_phaseStep(unsigned int freqHz) {
    /* Calcula cuánto debe avanzar la fase en cada muestra de audio */
    unsigned long long step = ((unsigned long long)freqHz << 32) / (unsigned long long)SAMPLE_RATE;
    return (unsigned int)step;
}

 /*
 * -----------------------------------------------------------------------------
 *  generate_tone
 * -----------------------------------------------------------------------------
 * 
 *  ATENCIÓN: esta es la función que debe pasar a ensamblador 80386.
 * 
 *  Rutina que genera una señal senoidal discreta y la almacena en un buffer
 *  de salida en formato PCM de 16 bits con signo.
 *
 *  La generación se basa en un acumulador de fase de 32 bits (phase), el cual
 *  se incrementa en cada muestra por un valor phaseStep proporcional a la
 *  frecuencia deseada. El índice para acceder a la tabla de senos se obtiene
 *  a partir de los 8 bits más significativos del acumulador de fase.
 *
 *  El cálculo de la amplitud se realiza usando aritmética de punto fijo Q15:
 *    - sineTable[] contiene valores Q15 en el rango [-32768, 32767]
 *    - amp representa la amplitud en formato Q15
 *    - La multiplicación Q15 × Q15 produce un resultado Q30
 *    - Un desplazamiento aritmético a la derecha de 15 bits retorna el valor
 *      al formato Q15 para su almacenamiento como muestra PCM de 16 bits
 *
 * -----------------------------------------------------------------------------
 */

void generate_tone(short* out, int nSamples, unsigned int freqHz, short amp) {
    unsigned int phase = 0;
    unsigned int phaseStep;
    int temp;
    short sample;
    int i;
    unsigned char index; 


    phaseStep = freq_to_phaseStep(freqHz);

    for ( i = 0; i < nSamples; i++) {

        /* Índice 0..255: bits más altos del acumulador */
        /* Se pierde un poco de precision pero la tabla de senos solo tiene 255 entradas */
        index = (phase >> 24);
        /* amp es La amplitud del tono, es decir, qué tan fuerte suena. en Q15*/
        /* 0: No suena, 1 (Q15: 32767) amplitud maxima */
        /* Q15 * Q15 = Q30  */
        temp = sineTable[index] * amp;

        /* Regresar a Q15 (desplazamiento aritmético porque temp es con signo) */
         sample = (temp >> 15);  
        out[i] = sample;

        /* Avanzar fase (wrap natural en unsigned int) */
        phase += phaseStep;
    }

}



// TRADUCCIÓN DE generate_tone a ensamblador:
__declspec(naked) void generate_tone_asm(short* out, int nSamples, unsigned int freqHz, unsigned short amp)
{
    __asm__ __volatile__(

        ".intel_syntax noprefix\n"

        "push ebp\n"
        "mov ebp, esp\n"
        "sub esp, 20\n"

        "mov dword ptr [ebp-4], 0\n"        // phase = 0
        "push dword ptr [ebp+16]\n"         // freqHz
        "call freq_to_phaseStep\n"          // phaseStep = freq_to_phaseStep(freqHz)
        "add esp, 4\n"                      // limpiar la pila
        "mov dword ptr [ebp-8], eax\n"      // guardar phaseStep
        "mov dword ptr [ebp-16], 0\n"       // i = 0

        "inicio_loop:\n"

        "mov eax, dword ptr [ebp-16]\n"     // i
        "cmp eax, dword ptr [ebp+12]\n"     // comparar i con nSamples
        "jge fin_loop\n"                    // si i >= nSamples, salir del loop

        "mov eax, dword ptr [ebp-4]\n"      // phase
        "shr eax, 24\n"                     // index = phase >> 24

        "movsx ecx, word ptr [sineTable + eax*2]\n"   // ecx = sineTable[index]
        "movzx edx, word ptr [ebp+20]\n"              // amp

        "imul ecx, edx\n"                   // ecx = ecx * edx

        "mov dword ptr [ebp-12], ecx\n"     // guardar temp

        "sar ecx, 15\n"                     // sample = temp >> 15; sar es desplazamiento aritmético

        "mov edx, dword ptr [ebp+8]\n"      // out
        "mov eax, dword ptr [ebp-16]\n"     // i
        "mov word ptr [edx + eax*2], cx\n"

        "mov eax, dword ptr [ebp-4]\n"      // phase
        "add eax, dword ptr [ebp-8]\n"      // phase += phaseStep
        "mov dword ptr [ebp-4], eax\n"      // guardar phase

        "inc dword ptr [ebp-16]\n"          // i++
        "jmp inicio_loop\n"

        "fin_loop:\n"

        "mov esp, ebp\n"
        "pop ebp\n"
        "ret\n"

        ".att_syntax\n"
    );
}



static int parse_positive_int(const char* s) {
    int v = 0;
    int i = 0;
    if (!s || s[0] == 0) return -1;
    while (s[i] != 0) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
        i++;
    }
    return v;
}

int main(int argc, char* argv[]) {
    /* Uso: TonoWav.exe <frecuencia_Hz> <duracion_ms> <salida.wav> */
    if (argc != 4) {
        printf("Uso: %s <frecuencia_Hz> <duracion_ms> <salida.wav>\n", argv[0]);
        printf("Ejemplo: %s 440 2000 tono.wav\n", argv[0]);
        return 1;
    }

    int freq = parse_positive_int(argv[1]);
    int durMs = parse_positive_int(argv[2]);

    if (freq <= 0 || durMs <= 0) {
        printf("Error: frecuencia y duracion deben ser enteros positivos.\n");
        return 1;
    }

    /* Puedes restringir aquí a un rango musical razonable */
    if (freq < 50 || freq > 2000) {
        printf("Error: frecuencia fuera de rango (50..2000 Hz recomendado).\n");
        return 1;
    }

    int nSamples = (int)(((unsigned long)SAMPLE_RATE * (unsigned long)durMs) / 1000UL);

    unsigned long dataSize = (unsigned long)nSamples * 2UL; /* 2 bytes por muestra (16-bit mono) */

    WavHeader h = {
        {'R','I','F','F'},
        36UL + dataSize,
        {'W','A','V','E'},
        {'f','m','t',' '},
        16UL,
        1,
        CHANNELS,
        SAMPLE_RATE,
        (unsigned long)(SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8)),
        (unsigned short)(CHANNELS * (BITS_PER_SAMPLE / 8)),
        BITS_PER_SAMPLE,
        {'d','a','t','a'},
        dataSize
    };

    FILE* f = fopen(argv[3], "wb");
    if (!f) {
        printf("Error: no se pudo crear el archivo '%s'\n", argv[3]);
        return 1;
    }

    if (fwrite(&h, sizeof(h), 1, f) != 1) {
        printf("Error: no se pudo escribir el header.\n");
        fclose(f);
        return 1;
    }

    /* Buffer de muestras */
    /* Si compilas como C++ (.cpp), mantén el cast (short*) */
    short* buf = (short*)malloc((unsigned int)nSamples * (unsigned int)sizeof(short));
    if (!buf) {
        printf("Error: sin memoria.\n");
        fclose(f);
        return 1;
    }

    /* Amplitud (volumen): 0..32767 recomendado */
    short amp = 25000;

    generate_tone(buf, nSamples, (unsigned long)freq, amp);

    if (fwrite(buf, sizeof(short), (unsigned long)nSamples, f) != (unsigned long)nSamples) {
        printf("Error: no se pudieron escribir muestras.\n");
        free(buf);
        fclose(f);
        return 1;
    }

    free(buf);
    fclose(f);

    printf("Generado: %s (freq=%d Hz, dur=%d ms)\n", argv[3], freq, durMs);
    return 0;
}



