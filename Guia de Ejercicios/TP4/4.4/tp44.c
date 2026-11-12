#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#define RUTA_FIFO "/tmp/tdiii_pipe"

struct Linea {
    char *texto;
    size_t largo;
    size_t capacidad;
};

static volatile sig_atomic_t salir;

/* Operación: pide terminar la espera. Entrada: señal. Salida: bandera salir. */
static void pedir_fin(int senal)
{
    (void)senal;
    salir = 1;
}

/* Operación: muestra un mensaje y vacía su buffer. Entrada: origen y línea.
 * Salida: línea en stdout precedida por el origen.
 */
static void mostrar(const char *origen, struct Linea *linea)
{
    printf("%s ", origen);
    if (linea->largo > 0)
        fwrite(linea->texto, 1, linea->largo, stdout);
    putchar('\n');
    fflush(stdout);
    linea->largo = 0;
}

/* Operación: lee lo que haya disponible sin esperar a que termine la línea.
 * Entrada: descriptor listo, origen y buffer. Salida: imprime cada mensaje
 * completo y guarda el resto. Devuelve 1 si leyó, 0 en EOF o -1 ante error.
 */
static int recibir(int fd, const char *origen, struct Linea *linea)
{
    char datos[512];
    ssize_t cantidad = read(fd, datos, sizeof(datos));
    if (cantidad == -1 && (errno == EINTR || errno == EAGAIN))
        return 1;
    if (cantidad <= 0)
        return cantidad;

    for (ssize_t i = 0; i < cantidad; i++) {
        if (datos[i] == '\n' || datos[i] == '\0') {
            mostrar(origen, linea);
        } else {
            if (linea->largo == linea->capacidad) {
                size_t nueva = 512;
                if (linea->capacidad > 0)
                    nueva = linea->capacidad * 2;
                if (nueva <= linea->capacidad) {
                    errno = ENOMEM;
                    return -1;
                }
                char *texto = realloc(linea->texto, nueva);
                if (texto == NULL)
                    return -1;
                linea->texto = texto;
                linea->capacidad = nueva;
            }
            linea->texto[linea->largo] = datos[i];
            linea->largo++;
        }
    }
    return 1;
}

/* Operación: crea la FIFO y espera con select a la FIFO y a stdin.
 * Entrada: líneas por ambas fuentes. Salida: mensajes en stdout y código de salida.
 */
int main(void)
{
    struct sigaction accion = {0};
    accion.sa_handler = pedir_fin;
    sigemptyset(&accion.sa_mask);
    if (sigaction(SIGINT, &accion, NULL) == -1 ||
        sigaction(SIGTERM, &accion, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    int leer_teclado = fcntl(STDIN_FILENO, F_GETFD) != -1;
    int creada = mkfifo(RUTA_FIFO, 0600) == 0;
    if (!creada && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }

    /* O_RDWR evita el EOF de la FIFO cuando se desconecta el último escritor. */
    int fifo = open(RUTA_FIFO, O_RDWR | O_NONBLOCK | O_NOFOLLOW);
    struct stat info;
    int resultado = 1;
    struct Linea teclado = {0}, pipe = {0};

    if (fifo == -1 || fstat(fifo, &info) == -1) {
        perror(RUTA_FIFO);
        goto cerrar;
    }
    if (!S_ISFIFO(info.st_mode) || fifo >= FD_SETSIZE) {
        fprintf(stderr, "La ruta no es una FIFO válida para select.\n");
        goto cerrar;
    }

    while (!salir) {
        fd_set lectura;
        struct timeval espera = {1, 0};
        FD_ZERO(&lectura);
        FD_SET(fifo, &lectura);
        if (leer_teclado)
            FD_SET(STDIN_FILENO, &lectura);

        int listos = select(fifo + 1, &lectura, NULL, NULL, &espera);
        if (listos == -1) {
            if (errno == EINTR)
                continue;
            perror("select");
            goto cerrar;
        }
        if (listos == 0)
            continue;

        if (FD_ISSET(fifo, &lectura) && recibir(fifo, "[PIPE]:", &pipe) == -1) {
            perror("read FIFO");
            goto cerrar;
        }
        if (leer_teclado && FD_ISSET(STDIN_FILENO, &lectura)) {
            int estado = recibir(STDIN_FILENO, "[KEYB]:", &teclado);
            if (estado == -1) {
                perror("read stdin");
                goto cerrar;
            }
            if (estado == 0) {
                if (teclado.largo > 0)
                    mostrar("[KEYB]:", &teclado);
                leer_teclado = 0;
            }
        }
    }
    resultado = 0;

cerrar:
    free(teclado.texto);
    free(pipe.texto);
    if (fifo != -1)
        close(fifo);
    if (creada)
        unlink(RUTA_FIFO);
    return resultado;
}
