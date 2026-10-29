/* Cliente basado en clieinet.c: gethostbyname, socket, connect, send y recv. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "ui.h"

static volatile sig_atomic_t terminar = 0;
static void detener(int senal)
{
    (void)senal;
    terminar = 1;
}

int main(int argc, char **argv)
{
    int descriptor, puerto = PUERTO_PREDETERMINADO, resultado, tecla, n;
    int conectado = 1, saliendo = 0, fallo = 0, registrado = 0;
    time_t inicio_salida = 0, ultimo_envio = time(NULL);
    size_t pendientes;
    struct hostent *servidor;
    struct sockaddr_in direccion;
    struct sigaction accion;
    struct timeval espera;
    fd_set lectura, escritura;
    Receptor receptor = {{0}, 0};
    Salida salida = {{0}, 0};
    char linea[TAM_LINEA], mensaje[TAM_LINEA], motivo[160] = "Conexion cerrada.";

    if (argc != 3 && argc != 4)
    {
        fprintf(stderr,
                "Uso: %s servidor nombre [puerto]\nNombre: 1 a 31 letras ASCII, numeros, guion o "
                "guion bajo.\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    if (!nombre_valido(argv[2]))
    {
        fprintf(stderr, "Nombre invalido: usa de 1 a 31 letras, numeros, guion o guion bajo.\n");
        return EXIT_FAILURE;
    }
    if (argc == 4)
    {
        puerto = puerto_valido(argv[3]);
        if (puerto == -1)
        {
            fprintf(stderr, "Puerto invalido.\n");
            return EXIT_FAILURE;
        }
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
    {
        fprintf(stderr, "El cliente ncurses necesita una terminal interactiva.\n");
        return EXIT_FAILURE;
    }
    /* 1. Obtener la IP del servidor, como en clieinet.c. */
    servidor = gethostbyname(argv[1]);
    if (servidor == NULL)
    {
        fprintf(stderr, "No se pudo resolver el servidor: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    /* 2. Crear el socket TCP y preparar la direccion de destino. */
    descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }
    if (descriptor >= FD_SETSIZE)
    {
        close(descriptor);
        return EXIT_FAILURE;
    }
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((unsigned short)puerto);
    memcpy(&direccion.sin_addr, servidor->h_addr_list[0], sizeof(direccion.sin_addr));
    /* 3. Conectarse. Las cartas y los resultados se recibiran del servidor. */
    if (connect(descriptor, (struct sockaddr *)&direccion, sizeof(direccion)) == -1)
    {
        perror("connect");
        close(descriptor);
        return EXIT_FAILURE;
    }
    memset(&accion, 0, sizeof(accion));
    sigemptyset(&accion.sa_mask);
    accion.sa_handler = detener;
    if (sigaction(SIGINT, &accion, NULL) == -1 || sigaction(SIGTERM, &accion, NULL) == -1)
    {
        perror("sigaction");
        close(descriptor);
        return EXIT_FAILURE;
    }
    if (ui_iniciar(argv[2]) == -1)
    {
        close(descriptor);
        return EXIT_FAILURE;
    }
    snprintf(mensaje, sizeof(mensaje), "UNIRSE %s\n", argv[2]);
    (void)encolar(&salida, mensaje);

    while (conectado && !terminar)
    {
        FD_ZERO(&lectura);
        FD_ZERO(&escritura);
        FD_SET(descriptor, &lectura);
        if (!saliendo)
        {
            FD_SET(STDIN_FILENO, &lectura);
        }
        if (salida.usados)
        {
            FD_SET(descriptor, &escritura);
        }
        espera.tv_sec = 0;
        espera.tv_usec = 200000;
        ui_dibujar();
        resultado = select(descriptor + 1, &lectura, &escritura, NULL, &espera);
        if (resultado == -1)
        {
            if (errno != EINTR)
            {
                snprintf(motivo, sizeof(motivo), "Error en select: %s", strerror(errno));
                fallo = 1;
                break;
            }
        }
        else
        {
            /* Enviar las acciones que quedaron pendientes de la vuelta anterior. */
            if (FD_ISSET(descriptor, &escritura))
            {
                pendientes = salida.usados;
                if (vaciar_salida(descriptor, &salida) == -1)
                {
                    snprintf(motivo, sizeof(motivo), "No se pudo enviar al servidor.");
                    fallo = 1;
                    break;
                }
                if (salida.usados < pendientes)
                {
                    ultimo_envio = time(NULL);
                }
            }
            /* Recibir mensajes del servidor y actualizar la pantalla. */
            if (FD_ISSET(descriptor, &lectura))
            {
                for (n = 0; n < 64; n++)
                {
                    resultado = recibir_linea(descriptor, &receptor, linea);
                    if (resultado == 2)
                    {
                        break;
                    }
                    if (resultado <= 0)
                    {
                        conectado = 0;
                        if (resultado == -1)
                        {
                            fallo = 1;
                        }
                        break;
                    }
                    ui_recibir(linea);
                    if (strncmp(linea, "BIENVENIDO ", 11) == 0)
                    {
                        registrado = 1;
                    }
                    if (!registrado && (strncmp(linea, "ERROR ", 6) == 0))
                    {
                        snprintf(motivo, sizeof(motivo), "Registro rechazado: %.130s", linea + 6);
                        conectado = 0;
                        fallo = 1;
                        break;
                    }
                    if (strcmp(linea, "SERVIDOR_LLENO") == 0)
                    {
                        snprintf(motivo, sizeof(motivo),
                                 "La mesa esta llena. Intenta cuando haya un lugar libre.");
                        fallo = 1;
                    }
                    else if (strcmp(linea, "SERVIDOR_CERRADO") == 0)
                    {
                        snprintf(motivo, sizeof(motivo), "El master cerro el servidor.");
                    }
                    else if (strcmp(linea, "HASTA_LUEGO") == 0)
                    {
                        snprintf(motivo, sizeof(motivo), "Saliste de la mesa.");
                    }
                }
            }
            /* getch() tambien puede tener teclas ya guardadas en el buffer de ncurses. */
            for (n = 0; n < 32 && !saliendo && conectado; n++)
            {
                const char *comando = NULL;
                tecla = ui_tecla();
                if (tecla == -1)
                {
                    break;
                }
                if (tecla == 'h' || tecla == 'H')
                {
                    comando = "PEDIR\n";
                }
                else if (tecla == 's' || tecla == 'S')
                {
                    comando = "PLANTARSE\n";
                }
                else if (tecla == 'i' || tecla == 'I')
                {
                    comando = "INICIAR\n";
                }
                else if (tecla == 'q' || tecla == 'Q')
                {
                    comando = "SALIR\n";
                    saliendo = 1;
                    inicio_salida = time(NULL);
                    ui_aviso("Saliendo de la mesa...");
                }
                if (comando)
                {
                    if (!salida.usados)
                    {
                        ultimo_envio = time(NULL);
                    }
                    if (encolar(&salida, comando) == -1)
                    {
                        fallo = 1;
                        terminar = 1;
                        break;
                    }
                }
            }
            if (saliendo && time(NULL) - inicio_salida >= 3)
            {
                break;
            }
            if (salida.usados && time(NULL) - ultimo_envio >= 10)
            {
                snprintf(motivo, sizeof(motivo), "El servidor no recibe los mensajes.");
                fallo = 1;
                break;
            }
        }
    }
    if (terminar)
    {
        (void)enviar_todo(descriptor, "SALIR\n");
    }
    ui_finalizar();
    close(descriptor);
    printf("%s\n", motivo);
    if (fallo)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
