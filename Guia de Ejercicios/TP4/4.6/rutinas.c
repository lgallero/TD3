#include <emmintrin.h>
#include <stddef.h>

#define MUESTRAS_SIMD 4
#define COEFICIENTES_FIR 4
#define CANALES 2

static unsigned char guardar_muestra(float valor)
{
    if (valor < 0.0f)
        valor = 0.0f;
    if (valor > 255.0f)
        valor = 255.0f;
    return (unsigned char)valor;
}

static __m128 cargar_cuatro(const unsigned char *audio)
{
    float muestras[MUESTRAS_SIMD];
    for (int i = 0; i < MUESTRAS_SIMD; i++)
        muestras[i] = audio[i];
    return _mm_loadu_ps(muestras);
}

static void guardar_cuatro(__m128 vector, unsigned char *salida)
{
    float muestras[MUESTRAS_SIMD];
    _mm_storeu_ps(muestras, vector);
    for (int i = 0; i < MUESTRAS_SIMD; i++)
        salida[i] = guardar_muestra(muestras[i]);
}

void echo(const unsigned char *entrada, unsigned char *salida,
          size_t bytes, size_t distancia)
{
    const unsigned char *anterior = salida - distancia;
    const __m128 quitar_silencio = _mm_set1_ps(-128.0f);
    const __m128 mitad = _mm_set1_ps(0.5f);
    size_t i = 0;

    for (; i + MUESTRAS_SIMD <= bytes; i += MUESTRAS_SIMD) {
        __m128 actual = cargar_cuatro(entrada + i);
        __m128 eco = cargar_cuatro(anterior + i);

        eco = _mm_add_ps(eco, quitar_silencio);
        eco = _mm_mul_ps(eco, mitad);
        __m128 resultado = _mm_add_ps(actual, eco);
        guardar_cuatro(resultado, salida + i);
    }

    for (; i < bytes; i++) {
        float eco = (anterior[i] - 128.0f) * 0.5f;
        salida[i] = guardar_muestra(entrada[i] + eco);
    }
}

void fir_low_pass(const unsigned char *entrada, unsigned char *salida,
                  size_t bytes)
{
    const float coeficientes[COEFICIENTES_FIR] = {0.25f, 0.25f, 0.25f, 0.25f};
    size_t i = 0;

    for (; i + MUESTRAS_SIMD <= bytes; i += MUESTRAS_SIMD) {
        __m128 suma = _mm_setzero_ps();

        for (int j = 0; j < COEFICIENTES_FIR; j++) {

            const unsigned char *muestras = entrada + i - j * CANALES;
            __m128 datos = cargar_cuatro(muestras);
            __m128 coef = _mm_set1_ps(coeficientes[j]);
            __m128 producto = _mm_mul_ps(datos, coef);
            suma = _mm_add_ps(suma, producto);
        }

        guardar_cuatro(suma, salida + i);
    }

    for (; i < bytes; i++) {
        float suma = 0.0f;
        for (int j = 0; j < COEFICIENTES_FIR; j++) {
            const unsigned char *muestra = entrada + i - j * CANALES;
            suma += *muestra * coeficientes[j];
        }
        salida[i] = guardar_muestra(suma);
    }
}

void delay(const unsigned char *entrada, unsigned char *salida,
           size_t bytes, size_t distancia)
{
    const unsigned char *anterior = entrada - distancia;
    size_t i = 0;

    for (; i + MUESTRAS_SIMD <= bytes; i += MUESTRAS_SIMD) {
        __m128 muestras = cargar_cuatro(anterior + i);
        guardar_cuatro(muestras, salida + i);
    }

    for (; i < bytes; i++)
        salida[i] = anterior[i];
}
