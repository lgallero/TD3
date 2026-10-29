#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stddef.h>

#define PUERTO_PREDETERMINADO 3490
#define TAM_LINEA 512
#define TAM_COLA 32768

/* Cada descriptor conserva su propia linea incompleta. */
typedef struct { char datos[TAM_LINEA]; size_t usados; } Receptor;
/* Cola pequena y acotada para no bloquear a todos por un cliente lento. */
typedef struct { char datos[TAM_COLA]; size_t usados; } Salida;

/* 1: linea lista sin '\n'; 2: incompleta; 0: cierre; -1: error. */
int recibir_linea(int descriptor, Receptor *receptor, char *linea);
int encolar(Salida *salida, const char *texto);
/* Envia lo posible sin bloquear. 0: correcto (puede quedar pendiente); -1: error. */
int vaciar_salida(int descriptor, Salida *salida);
/* Solo para mensajes pequenos de inicio/cierre, nunca para el broadcast. */
int enviar_todo(int descriptor, const char *texto);
int puerto_valido(const char *texto);
int nombre_valido(const char *nombre);
#endif
