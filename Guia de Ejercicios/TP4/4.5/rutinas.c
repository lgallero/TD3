#include <stddef.h>
#include <string.h>

/* Operación: paso directo para probar el hijo de eco del 4.5.
 * Entrada: audio, longitud y distancia del eco. Salida: copia del audio.
 */
void echo(const unsigned char *entrada, unsigned char *salida,
          size_t bytes, size_t distancia)
{
    (void)distancia;
    memcpy(salida, entrada, bytes);
}

/* Operación: paso directo para probar el hijo del FIR del 4.5.
 * Entrada: audio y longitud. Salida: copia del audio.
 */
void fir_low_pass(const unsigned char *entrada, unsigned char *salida, size_t bytes)
{
    memcpy(salida, entrada, bytes);
}

/* Operación: paso directo para probar el hijo de retardo del 4.5.
 * Entrada: audio, longitud y distancia del retardo. Salida: copia del audio.
 */
void delay(const unsigned char *entrada, unsigned char *salida,
           size_t bytes, size_t distancia)
{
    (void)distancia;
    memcpy(salida, entrada, bytes);
}
