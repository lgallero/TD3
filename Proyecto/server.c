/* UTN FRH - Tecnicas Digitales III. Padre master + un puente hijo por cliente. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "blackjack.h"
#include "protocol.h"

#define COLA_CONEXIONES 10

typedef struct
{
    int descriptor;
    int cerrando;
    int fallo;
    time_t ingreso;
    time_t ultimo_envio;
    Receptor receptor;
    Salida salida;
} CanalHijo;

static CanalHijo canales[MAX_JUGADORES];
static fd_set conjunto_master;
static volatile sig_atomic_t terminar = 0;

static void detener(int senal)
{
    (void)senal;
    terminar = 1;
}

static void recoger_hijos(int senal)
{
    int guardado = errno;
    pid_t pid;
    (void)senal;
    do
    {
        pid = waitpid(-1, NULL, WNOHANG);
    } while (pid > 0 || (pid == -1 && errno == EINTR));
    errno = guardado;
}

/* Ningun envio de estados bloquea al master. select() vacia las colas. */
static void enviar_mensaje(int id, const char *mensaje)
{
    if (canales[id].descriptor < 0 || canales[id].cerrando)
    {
        return;
    }
    if (!canales[id].salida.usados)
    {
        canales[id].ultimo_envio = time(NULL);
    }
    if (encolar(&canales[id].salida, mensaje) == -1)
    {
        canales[id].fallo = 1;
    }
}

/* Equivale a broadcast_to_children(): el padre publica a todos los registrados. */
static void difundir(const char *mensaje)
{
    int i;
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado)
        {
            enviar_mensaje(i, mensaje);
        }
    }
}

/* Publicar la misma mesa a todos; la carta oculta nunca se envia. */
static void publicar_estado(void)
{
    int i;
    int ocultar = 0;
    int visibles = dealer.cantidad_cartas;
    const Carta *mano_visible = dealer.mano;
    char linea[TAM_LINEA];
    char cartas[128];

    if (estado_ronda == RONDA_ACTIVA)
    {
        ocultar = 1;
        if (dealer.cantidad_cartas > 0)
        {
            mano_visible = &dealer.mano[1];
            visibles = dealer.cantidad_cartas - 1;
        }
    }

    snprintf(linea, sizeof(linea), "ESTADO %d %d %d\n", numero_ronda, estado_ronda, turno_actual);
    difundir(linea);
    texto_cartas(mano_visible, visibles, cartas, sizeof(cartas));
    snprintf(linea, sizeof(linea), "DEALER %d %d %s\n", calcular_puntaje(mano_visible, visibles),
             ocultar, cartas);
    difundir(linea);

    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado)
        {
            texto_cartas(jugadores[i].mano, jugadores[i].cantidad_cartas, cartas, sizeof(cartas));
            snprintf(linea, sizeof(linea), "JUGADOR %d %s %d %s %s %s\n", i,
                     nombre_estado(jugadores[i].estado),
                     calcular_puntaje(jugadores[i].mano, jugadores[i].cantidad_cartas),
                     jugadores[i].nombre, nombre_resultado(jugadores[i].resultado), cartas);
            difundir(linea);
        }
    }
    difundir("FIN_ESTADO\n");

    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado)
        {
            if (jugadores[i].estado == JUGADOR_ESPERANDO)
            {
                enviar_mensaje(i, "ESPERANDO\n");
            }
            else if (estado_ronda == RONDA_ACTIVA && turno_actual == i)
            {
                enviar_mensaje(i, "TU_TURNO\n");
            }
            else
            {
                enviar_mensaje(i, "ESPERAR\n");
            }
        }
    }
}

static void anunciar_cambio(int estado_anterior)
{
    int i;
    char linea[TAM_LINEA];
    if (estado_anterior == RONDA_ACTIVA && estado_ronda == RONDA_TERMINADA)
    {
        difundir("TURNO_DEALER\n");
        for (i = 0; i < MAX_JUGADORES; i++)
        {
            if (jugadores[i].conectado && jugadores[i].participa)
            {
                snprintf(linea, sizeof(linea), "RESULTADO %s %s\n", jugadores[i].nombre,
                         nombre_resultado(jugadores[i].resultado));
                difundir(linea);
            }
        }
        difundir("RONDA_FINALIZADA\n");
    }
    if (estado_ronda == RONDA_ACTIVA)
    {
        snprintf(linea, sizeof(linea), "TURNO %s\n", jugadores[turno_actual].nombre);
        difundir(linea);
    }
    publicar_estado();
}

static void retirar_del_juego(int id)
{
    char linea[TAM_LINEA];
    int anterior = estado_ronda;
    if (!jugadores[id].conectado)
    {
        return;
    }
    snprintf(linea, sizeof(linea), "JUGADOR_DESCONECTADO %s\n", jugadores[id].nombre);
    desconectar_jugador(id);
    difundir(linea);
    anunciar_cambio(anterior);
}

static void cerrar_canal(int id)
{
    if (canales[id].descriptor < 0)
    {
        return;
    }
    FD_CLR(canales[id].descriptor, &conjunto_master);
    close(canales[id].descriptor);
    canales[id].descriptor = -1;
    retirar_del_juego(id);
    printf("Master: canal %d cerrado.\n", id);
}

/* Deja salir la despedida antes de cerrar el IPC. */
static void despedir(int id, const char *mensaje)
{
    enviar_mensaje(id, mensaje);
    canales[id].cerrando = 1;
    canales[id].ingreso = time(NULL);
    retirar_del_juego(id);
}

/* Se ejecuta cuando el cliente aun no tiene nombre registrado. */
static void registrar_cliente(int id, const char *accion)
{
    char linea[TAM_LINEA];
    int i;

    if (strncmp(accion, "UNIRSE ", 7) != 0 || !nombre_valido(accion + 7))
    {
        enviar_mensaje(
            id, "ERROR Usa UNIRSE y un nombre de 1 a 31 letras, numeros, guion o guion bajo.\n");
        return;
    }
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (jugadores[i].conectado && (strcmp(jugadores[i].nombre, accion + 7) == 0))
        {
            enviar_mensaje(id, "ERROR Nombre en uso.\n");
            return;
        }
    }
    registrar_jugador(id, accion + 7);
    snprintf(linea, sizeof(linea), "BIENVENIDO %d\n", id);
    enviar_mensaje(id, linea);
    snprintf(linea, sizeof(linea), "JUGADOR_UNIDO %s\n", jugadores[id].nombre);
    difundir(linea);
    publicar_estado();
    return;
}

static void procesar_accion(int id, const char *accion)
{
    char linea[TAM_LINEA];
    int anterior;
    if (strcmp(accion, "SALIR") == 0)
    {
        despedir(id, "HASTA_LUEGO\n");
        return;
    }
    if (!jugadores[id].conectado)
    {
        registrar_cliente(id, accion);
        return;
    }
    if (strcmp(accion, "INICIAR") == 0)
    {
        if (iniciar_ronda() == -1)
        {
            enviar_mensaje(id, "ERROR Ya hay una ronda activa.\n");
            return;
        }
        difundir("RONDA_INICIADA\n");
        anunciar_cambio(RONDA_ACTIVA);
        return;
    }
    if (strcmp(accion, "PEDIR") != 0 && strcmp(accion, "PLANTARSE") != 0)
    {
        enviar_mensaje(id, "ERROR Comando desconocido. Usa INICIAR, PEDIR, PLANTARSE o SALIR.\n");
        return;
    }
    if (estado_ronda != RONDA_ACTIVA)
    {
        enviar_mensaje(id, "ERROR No hay una ronda activa.\n");
        return;
    }
    if (turno_actual != id)
    {
        enviar_mensaje(id, "NO_ES_TU_TURNO\n");
        return;
    }
    anterior = estado_ronda;
    if (strcmp(accion, "PEDIR") == 0)
    {
        if (pedir_carta(id) == -1)
        {
            enviar_mensaje(id, "ERROR No podes pedir otra carta con esta mano.\n");
            return;
        }
        snprintf(linea, sizeof(linea), "JUGADOR_PIDE %s\n", jugadores[id].nombre);
    }
    else
    {
        (void)plantarse(id);
        snprintf(linea, sizeof(linea), "JUGADOR_PLANTADO %s\n", jugadores[id].nombre);
    }
    difundir(linea);
    anunciar_cambio(anterior);
}

/* HIJO: sin reglas del juego. Solo transporta lineas y avisa del cierre TCP. */
static void puente_cliente(int cliente, int padre, int id)
{
    Receptor entrada_cliente = {{0}, 0};
    Receptor entrada_padre = {{0}, 0};
    Salida hacia_cliente = {{0}, 0};
    Salida hacia_padre = {{0}, 0};
    fd_set lectura, escritura;
    struct timeval espera;
    char linea[TAM_LINEA], mensaje[TAM_LINEA + 32];
    int tcp_abierto = 1, ipc_abierto = 1, fallo = 0;
    int cantidad, resultado, n, maximo;
    time_t fin = 0, progreso = time(NULL);
    size_t pendientes;

    maximo = cliente;
    if (padre > maximo)
    {
        maximo = padre;
    }

    while (!terminar && !fallo)
    {
        /* Dejar salir lo pendiente antes de cerrar ambos extremos. */
        if (!tcp_abierto && hacia_padre.usados == 0)
        {
            break;
        }
        if (!ipc_abierto && hacia_cliente.usados == 0)
        {
            break;
        }
        if (fin != 0 && time(NULL) - fin >= 3)
        {
            break;
        }
        if ((hacia_cliente.usados > 0 || hacia_padre.usados > 0) && time(NULL) - progreso >= 10)
        {
            break;
        }

        FD_ZERO(&lectura);
        FD_ZERO(&escritura);
        if (tcp_abierto && ipc_abierto)
        {
            FD_SET(cliente, &lectura);
            FD_SET(padre, &lectura);
        }
        if (tcp_abierto && hacia_cliente.usados > 0)
        {
            FD_SET(cliente, &escritura);
        }
        if (ipc_abierto && hacia_padre.usados > 0)
        {
            FD_SET(padre, &escritura);
        }
        espera.tv_sec = 1;
        espera.tv_usec = 0;
        resultado = select(maximo + 1, &lectura, &escritura, NULL, &espera);

        if (resultado == -1)
        {
            if (errno != EINTR)
            {
                fallo = 1;
            }
        }
        else
        {
            /* 1. Padre -> hijo -> cliente. Incluye el rechazo por mesa llena. */
            if (FD_ISSET(padre, &lectura))
            {
                for (n = 0; n < 32 && !fallo; n++)
                {
                    cantidad = recibir_linea(padre, &entrada_padre, linea);
                    if (cantidad == 2)
                    {
                        break; /* Todavia falta completar la linea. */
                    }
                    if (cantidad <= 0)
                    {
                        ipc_abierto = 0;
                        hacia_padre.usados = 0;
                        fin = time(NULL);
                        break;
                    }
                    snprintf(mensaje, sizeof(mensaje), "%s\n", linea);
                    if (hacia_cliente.usados == 0)
                    {
                        progreso = time(NULL);
                    }
                    if (encolar(&hacia_cliente, mensaje) == -1)
                    {
                        fallo = 1;
                    }
                }
            }

            /* 2. Cliente -> hijo -> padre. El hijo no aplica reglas del juego. */
            if (!fallo && tcp_abierto && ipc_abierto && FD_ISSET(cliente, &lectura))
            {
                for (n = 0; n < 32 && !fallo; n++)
                {
                    cantidad = recibir_linea(cliente, &entrada_cliente, linea);
                    if (cantidad == 2)
                    {
                        break;
                    }
                    if (cantidad <= 0 || strlen(linea) > TAM_LINEA - 9)
                    {
                        tcp_abierto = 0;
                        hacia_cliente.usados = 0;
                        fin = time(NULL);
                        snprintf(mensaje, sizeof(mensaje), "DESCONECTADO %d\n", id);
                        if (encolar(&hacia_padre, mensaje) == -1)
                        {
                            fallo = 1;
                        }
                        break;
                    }
                    /* Todas las ordenes se identifican como acciones del cliente. */
                    snprintf(mensaje, sizeof(mensaje), "ACCION %s\n", linea);
                    if (hacia_padre.usados == 0)
                    {
                        progreso = time(NULL);
                    }
                    if (encolar(&hacia_padre, mensaje) == -1)
                    {
                        fallo = 1;
                    }
                }
            }

            /* 3. Enviar lo pendiente solo a los sockets listos para escribir. */
            if (!fallo)
            {
                pendientes = hacia_cliente.usados + hacia_padre.usados;
                if (tcp_abierto && FD_ISSET(cliente, &escritura))
                {
                    if (vaciar_salida(cliente, &hacia_cliente) == -1)
                    {
                        break;
                    }
                }
                if (ipc_abierto && FD_ISSET(padre, &escritura))
                {
                    if (vaciar_salida(padre, &hacia_padre) == -1)
                    {
                        ipc_abierto = 0;
                        hacia_padre.usados = 0;
                        fin = time(NULL);
                    }
                }
                if (hacia_cliente.usados + hacia_padre.usados < pendientes)
                {
                    progreso = time(NULL);
                }
            }
        }
    }
    close(cliente);
    close(padre);
}

static void aceptar_cliente(int escucha)
{
    int cliente, canal[2], i, id = -1;
    pid_t pid;
    struct sockaddr_in direccion;
    socklen_t tam = sizeof(direccion);
    cliente = accept(escucha, (struct sockaddr *)&direccion, &tam);
    if (cliente == -1)
    {
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("accept");
        }
        return;
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, canal) == -1)
    {
        perror("socketpair");
        close(cliente);
        return;
    }
    if (cliente >= FD_SETSIZE || canal[0] >= FD_SETSIZE || canal[1] >= FD_SETSIZE)
    {
        close(cliente);
        close(canal[0]);
        close(canal[1]);
        return;
    }
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (canales[i].descriptor < 0)
        {
            id = i;
            break;
        }
    }
    fflush(stdout);
    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        close(cliente);
        close(canal[0]);
        close(canal[1]);
        return;
    }
    if (pid == 0)
    {
        close(escucha);
        close(canal[0]);
        for (i = 0; i < MAX_JUGADORES; i++)
        {
            if (canales[i].descriptor >= 0)
            {
                close(canales[i].descriptor);
            }
        }
        signal(SIGCHLD, SIG_DFL);
        puente_cliente(cliente, canal[1], id);
        _exit(EXIT_SUCCESS);
    }
    close(cliente);
    close(canal[1]);
    printf("Master: conexion desde %s; hijo %ld; lugar %d.\n", inet_ntoa(direccion.sin_addr),
           (long)pid, id);
    if (id == -1)
    {
        /* El sexto cliente tambien es atendido por un hijo, sin ocupar jugador. */
        (void)enviar_todo(canal[0], "SERVIDOR_LLENO\n");
        close(canal[0]);
        return;
    }
    memset(&canales[id], 0, sizeof(canales[id]));
    canales[id].descriptor = canal[0];
    canales[id].ingreso = time(NULL);
    canales[id].ultimo_envio = canales[id].ingreso;
    FD_SET(canal[0], &conjunto_master);
    enviar_mensaje(id, "CONECTADO Envia UNIRSE nombre.\n");
}

/* Procesar envios, acciones y vencimientos de un unico jugador. */
static void atender_canal(int id, fd_set *lectura, fd_set *escritura)
{
    size_t pendientes;
    int n, cantidad;
    int debe_cerrar = 0;
    char linea[TAM_LINEA];
    if (FD_ISSET(canales[id].descriptor, escritura))
    {
        pendientes = canales[id].salida.usados;
        if (vaciar_salida(canales[id].descriptor, &canales[id].salida) == -1)
        {
            canales[id].fallo = 1;
        }
        if (canales[id].salida.usados < pendientes)
        {
            canales[id].ultimo_envio = time(NULL);
        }
    }
    if (!canales[id].cerrando && !canales[id].fallo && FD_ISSET(canales[id].descriptor, lectura))
    {
        for (n = 0; n < 32 && !canales[id].cerrando; n++)
        {
            cantidad = recibir_linea(canales[id].descriptor, &canales[id].receptor, linea);
            if (cantidad == 2)
            {
                break;
            }
            if (cantidad <= 0)
            {
                canales[id].fallo = 1;
                break;
            }
            if (strncmp(linea, "ACCION ", 7) == 0)
            {
                procesar_accion(id, linea + 7);
            }
            else
            {
                canales[id].fallo = 1;
                break;
            } /* DESCONECTADO o IPC invalido. */
        }
    }
    if (!canales[id].cerrando && !jugadores[id].conectado && time(NULL) - canales[id].ingreso >= 30)
    {
        despedir(id, "ERROR Tiempo para registrarse agotado.\n");
    }
    /* Se revisa cada motivo de cierre por separado. */
    if (canales[id].fallo)
    {
        debe_cerrar = 1;
    }
    if (canales[id].salida.usados > 0 && time(NULL) - canales[id].ultimo_envio >= 10)
    {
        debe_cerrar = 1;
    }
    if (canales[id].cerrando)
    {
        if (canales[id].salida.usados == 0 || time(NULL) - canales[id].ingreso >= 3)
        {
            debe_cerrar = 1;
        }
    }
    if (debe_cerrar)
    {
        cerrar_canal(id);
    }
}

int main(int argc, char **argv)
{
    int escucha, opcion = 1, puerto = PUERTO_PREDETERMINADO, i, maximo, res;
    struct sockaddr_in direccion;
    struct sigaction accion;
    struct timeval espera;
    fd_set lectura, escritura;
    unsigned int semilla = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    if (argc > 3)
    {
        fprintf(stderr, "Uso: %s [puerto [semilla_de_prueba]]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc >= 2)
    {
        puerto = puerto_valido(argv[1]);
        if (puerto == -1)
        {
            fprintf(stderr, "Puerto invalido.\n");
            return EXIT_FAILURE;
        }
    }
    if (argc == 3)
    {
        char *fin;
        unsigned long valor;
        errno = 0;
        valor = strtoul(argv[2], &fin, 10);
        if (errno || !*argv[2] || *fin || argv[2][0] == '-' || valor > 0xffffffffUL)
        {
            fprintf(stderr, "Semilla invalida.\n");
            return EXIT_FAILURE;
        }
        semilla = (unsigned int)valor;
    }
    srand(semilla);
    inicializar_juego();
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        canales[i].descriptor = -1;
    }
    memset(&accion, 0, sizeof(accion));
    sigemptyset(&accion.sa_mask);
    accion.sa_handler = recoger_hijos;
    accion.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &accion, NULL) == -1)
    {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    accion.sa_handler = detener;
    accion.sa_flags = 0;
    if (sigaction(SIGINT, &accion, NULL) == -1 || sigaction(SIGTERM, &accion, NULL) == -1)
    {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    /* 1. Crear TCP. La secuencia de red conserva la de servinet.c. */
    escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (escucha == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }
    if (escucha >= FD_SETSIZE)
    {
        fprintf(stderr, "El descriptor de escucha supera el limite de select.\n");
        close(escucha);
        return EXIT_FAILURE;
    }
    if (setsockopt(escucha, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion)) == -1)
    {
        perror("setsockopt");
        close(escucha);
        return EXIT_FAILURE;
    }
    if (fcntl(escucha, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("fcntl");
        close(escucha);
        return EXIT_FAILURE;
    }
    /* 2. Preparar la direccion IPv4 y el puerto local. */
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((unsigned short)puerto);
    direccion.sin_addr.s_addr = INADDR_ANY;
    /* 3. Asociar el socket a esa direccion con bind. */
    if (bind(escucha, (struct sockaddr *)&direccion, sizeof(direccion)) == -1)
    {
        perror("bind");
        close(escucha);
        return EXIT_FAILURE;
    }
    /* 4. Habilitar conexiones pendientes. accept y fork estan en aceptar_cliente. */
    if (listen(escucha, COLA_CONEXIONES) == -1)
    {
        perror("listen");
        close(escucha);
        return EXIT_FAILURE;
    }
    FD_ZERO(&conjunto_master);
    FD_SET(escucha, &conjunto_master);
    printf("Blackjack - Master PID %ld - TCP %d - hasta %d jugadores\n", (long)getpid(), puerto,
           MAX_JUGADORES);
    printf("Ctrl+C para finalizar. Semilla: %u\n", semilla);
    fflush(stdout);

    while (!terminar)
    {
        lectura = conjunto_master;
        FD_ZERO(&escritura);
        maximo = escucha;
        for (i = 0; i < MAX_JUGADORES; i++)
        {
            if (canales[i].descriptor >= 0)
            {
                if (canales[i].cerrando)
                {
                    FD_CLR(canales[i].descriptor, &lectura);
                }
                if (canales[i].salida.usados > 0)
                {
                    FD_SET(canales[i].descriptor, &escritura);
                }
                if (canales[i].descriptor > maximo)
                {
                    maximo = canales[i].descriptor;
                }
            }
        }
        espera.tv_sec = 1;
        espera.tv_usec = 0;
        res = select(maximo + 1, &lectura, &escritura, NULL, &espera);
        if (res == -1)
        {
            if (errno != EINTR)
            {
                perror("select master");
                break;
            }
        }
        else
        {
            /* Atender primero los jugadores existentes y despues las conexiones nuevas. */
            for (i = 0; i < MAX_JUGADORES; i++)
            {
                if (canales[i].descriptor >= 0)
                {
                    atender_canal(i, &lectura, &escritura);
                }
            }
            if (FD_ISSET(escucha, &lectura))
            {
                aceptar_cliente(escucha);
            }
        }
    }
    close(escucha);
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        if (canales[i].descriptor >= 0)
        {
            (void)encolar(&canales[i].salida, "SERVIDOR_CERRADO\n");
            (void)vaciar_salida(canales[i].descriptor, &canales[i].salida);
            close(canales[i].descriptor);
        }
    }
    /* Al cerrar IPC los puentes salen; su vaciado final tiene limite de 3 s. */
    while (waitpid(-1, NULL, 0) != -1 || errno == EINTR)
    {
    }
    printf("Master finalizado.\n");
    return EXIT_SUCCESS;
}
