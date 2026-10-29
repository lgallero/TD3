/* TCP y socketpair son flujos: las lineas pueden llegar juntas o fragmentadas. */
#include "protocol.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

int recibir_linea(int descriptor, Receptor *receptor, char *linea)
{
    char c;
    ssize_t cantidad;
    /* Un byte por vez evita perder el comienzo del mensaje siguiente. */
    while (1)
    {
        cantidad = recv(descriptor, &c, 1, MSG_DONTWAIT);
        if (cantidad == -1)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 2;
            return -1;
        }
        if (cantidad == 0)
        {
            if (receptor->usados) { errno = EPROTO; return -1; }
            return 0;
        }
        if (c == '\n')
        {
            if (receptor->usados && receptor->datos[receptor->usados - 1] == '\r')
                receptor->usados--;
            receptor->datos[receptor->usados] = '\0';
            memcpy(linea, receptor->datos, receptor->usados + 1);
            receptor->usados = 0;
            return 1;
        }
        if (c == '\0' || ((unsigned char)c < 32u && c != '\r'))
        { errno = EPROTO; return -1; }
        if (receptor->usados >= TAM_LINEA - 1)
        { errno = EMSGSIZE; return -1; }
        receptor->datos[receptor->usados++] = c;
    }
}

int encolar(Salida *salida, const char *texto)
{
    size_t cantidad = strlen(texto);
    if (cantidad > sizeof(salida->datos) - salida->usados)
    { errno = ENOBUFS; return -1; }
    memcpy(salida->datos + salida->usados, texto, cantidad);
    salida->usados += cantidad;
    return 0;
}

int vaciar_salida(int descriptor, Salida *salida)
{
    ssize_t cantidad;
    if (!salida->usados) return 0;
    cantidad = send(descriptor, salida->datos, salida->usados, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (cantidad == -1)
    {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    if (cantidad == 0) { errno = EPIPE; return -1; }
    salida->usados -= (size_t)cantidad;
    memmove(salida->datos, salida->datos + cantidad, salida->usados);
    return 0;
}

int enviar_todo(int descriptor, const char *texto)
{
    size_t enviados = 0, total = strlen(texto);
    ssize_t cantidad;
    /* Si se llena el socket, devuelve error: no bloquea el master. */
    while (enviados < total)
    {
        cantidad = send(descriptor, texto + enviados, total - enviados,
                        MSG_DONTWAIT | MSG_NOSIGNAL);
        if (cantidad == -1) { if (errno == EINTR) continue; return -1; }
        if (cantidad == 0) { errno = EPIPE; return -1; }
        enviados += (size_t)cantidad;
    }
    return 0;
}

int puerto_valido(const char *texto)
{
    char *fin;
    long puerto;
    errno = 0;
    puerto = strtol(texto, &fin, 10);
    if (errno || !*texto || *fin || puerto < 1 || puerto > 65535) return -1;
    return (int)puerto;
}

int nombre_valido(const char *nombre)
{
    size_t i, largo = strlen(nombre);
    if (largo < 1 || largo > 31) return 0;
    for (i = 0; i < largo; i++)
        if (!((nombre[i] >= 'A' && nombre[i] <= 'Z') ||
              (nombre[i] >= 'a' && nombre[i] <= 'z') ||
              (nombre[i] >= '0' && nombre[i] <= '9') ||
              (i > 0 && (nombre[i] == '_' || nombre[i] == '-')))) return 0;
    return 1;
}
