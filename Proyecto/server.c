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

typedef struct {
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

static void detener(int senal) { (void)senal; terminar = 1; }

static void recoger_hijos(int senal)
{
    int guardado = errno;
    pid_t pid;
    (void)senal;
    do { pid = waitpid(-1, NULL, WNOHANG); }
    while (pid > 0 || (pid == -1 && errno == EINTR));
    errno = guardado;
}

/* Ningun envio de estados bloquea al master. select() vacia las colas. */
static void enviar_mensaje(int id, const char *mensaje)
{
    if (canales[id].descriptor < 0 || canales[id].cerrando) return;
    if (!canales[id].salida.usados) canales[id].ultimo_envio = time(NULL);
    if (encolar(&canales[id].salida, mensaje) == -1) canales[id].fallo = 1;
}

/* Equivale a broadcast_to_children(): el padre publica a todos los registrados. */
static void difundir(const char *mensaje)
{
    int i;
    for (i = 0; i < MAX_JUGADORES; i++)
        if (jugadores[i].conectado) enviar_mensaje(i, mensaje);
}

static void publicar_estado(void)
{
    int i, ocultar = estado_ronda == RONDA_ACTIVA;
    int visibles = ocultar ? 1 : dealer.cantidad_cartas;
    char linea[TAM_LINEA], cartas[128];
    snprintf(linea, sizeof(linea), "ESTADO %d %d %d\n", numero_ronda, estado_ronda, turno_actual);
    difundir(linea);
    texto_cartas(dealer.mano, visibles, cartas, sizeof(cartas));
    snprintf(linea, sizeof(linea), "DEALER %d %d %s\n",
             calcular_puntaje(dealer.mano, visibles), ocultar, cartas);
    difundir(linea);
    for (i = 0; i < MAX_JUGADORES; i++)
    {
        Jugador *j = &jugadores[i];
        if (!j->conectado) continue;
        texto_cartas(j->mano, j->cantidad_cartas, cartas, sizeof(cartas));
        snprintf(linea, sizeof(linea), "JUGADOR %d %s %d %s %s %s\n", i,
                 nombre_estado(j->estado), calcular_puntaje(j->mano, j->cantidad_cartas),
                 j->nombre, nombre_resultado(j->resultado), cartas);
        difundir(linea);
    }
    difundir("FIN_ESTADO\n");
    for (i = 0; i < MAX_JUGADORES; i++)
        if (jugadores[i].conectado)
            enviar_mensaje(i, jugadores[i].estado == JUGADOR_ESPERANDO ? "ESPERANDO\n" :
                           (estado_ronda == RONDA_ACTIVA && turno_actual == i ? "TU_TURNO\n" : "ESPERAR\n"));
}

static void anunciar_cambio(int estado_anterior)
{
    int i;
    char linea[TAM_LINEA];
    if (estado_anterior == RONDA_ACTIVA && estado_ronda == RONDA_TERMINADA)
    {
        difundir("TURNO_DEALER\n");
        for (i = 0; i < MAX_JUGADORES; i++)
            if (jugadores[i].conectado && jugadores[i].participa)
            {
                snprintf(linea, sizeof(linea), "RESULTADO %s %s\n", jugadores[i].nombre,
                         nombre_resultado(jugadores[i].resultado));
                difundir(linea);
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
    if (!jugadores[id].conectado) return;
    snprintf(linea, sizeof(linea), "JUGADOR_DESCONECTADO %s\n", jugadores[id].nombre);
    desconectar_jugador(id);
    difundir(linea);
    anunciar_cambio(anterior);
}

static void cerrar_canal(int id)
{
    if (canales[id].descriptor < 0) return;
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

static void procesar_accion(int id, const char *accion)
{
    char linea[TAM_LINEA];
    int i, anterior;
    if (strcmp(accion, "SALIR") == 0) { despedir(id, "HASTA_LUEGO\n"); return; }
    if (!jugadores[id].conectado)
    {
        if (strncmp(accion, "UNIRSE ", 7) || !nombre_valido(accion + 7))
        { enviar_mensaje(id, "ERROR Usa UNIRSE y un nombre de 1 a 31 letras, numeros, guion o guion bajo.\n"); return; }
        for (i = 0; i < MAX_JUGADORES; i++)
            if (jugadores[i].conectado && !strcmp(jugadores[i].nombre, accion + 7))
            { enviar_mensaje(id, "ERROR Nombre en uso.\n"); return; }
        registrar_jugador(id, accion + 7);
        snprintf(linea, sizeof(linea), "BIENVENIDO %d\n", id);
        enviar_mensaje(id, linea);
        snprintf(linea, sizeof(linea), "JUGADOR_UNIDO %s\n", jugadores[id].nombre);
        difundir(linea);
        publicar_estado();
        return;
    }
    if (!strcmp(accion, "INICIAR"))
    {
        if (iniciar_ronda() == -1)
        { enviar_mensaje(id, "ERROR Ya hay una ronda activa.\n"); return; }
        difundir("RONDA_INICIADA\n");
        anunciar_cambio(RONDA_ACTIVA);
        return;
    }
    if (strcmp(accion, "PEDIR") && strcmp(accion, "PLANTARSE"))
    { enviar_mensaje(id, "ERROR Comando desconocido. Usa INICIAR, PEDIR, PLANTARSE o SALIR.\n"); return; }
    if (estado_ronda != RONDA_ACTIVA)
    { enviar_mensaje(id, "ERROR No hay una ronda activa.\n"); return; }
    if (turno_actual != id)
    { enviar_mensaje(id, "NO_ES_TU_TURNO\n"); return; }
    anterior = estado_ronda;
    if (!strcmp(accion, "PEDIR"))
    {
        (void)pedir_carta(id);
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
    Receptor entrada_cliente = {{0}, 0}, entrada_padre = {{0}, 0};
    Salida hacia_cliente = {{0}, 0}, hacia_padre = {{0}, 0};
    fd_set lectura, escritura;
    struct timeval espera;
    char linea[TAM_LINEA], mensaje[TAM_LINEA + 32];
    int tcp_abierto = 1, ipc_abierto = 1, cantidad, res, n;
    time_t fin = 0, progreso = time(NULL);
    size_t pendientes;

    while (!terminar)
    {
        if ((!tcp_abierto && !hacia_padre.usados) || (!ipc_abierto && !hacia_cliente.usados)) break;
        if (fin && time(NULL) - fin >= 3) break;
        if ((hacia_cliente.usados || hacia_padre.usados) && time(NULL) - progreso >= 10) break;
        FD_ZERO(&lectura); FD_ZERO(&escritura);
        if (tcp_abierto && ipc_abierto) { FD_SET(cliente, &lectura); FD_SET(padre, &lectura); }
        if (tcp_abierto && hacia_cliente.usados) FD_SET(cliente, &escritura);
        if (ipc_abierto && hacia_padre.usados) FD_SET(padre, &escritura);
        espera.tv_sec = 1; espera.tv_usec = 0;
        res = select((cliente > padre ? cliente : padre) + 1, &lectura, &escritura, NULL, &espera);
        if (res == -1) { if (errno == EINTR) continue; break; }

        /* Primero lee al padre: tambien permite entregar SERVIDOR_LLENO. */
        if (FD_ISSET(padre, &lectura))
            for (n = 0; n < 32; n++)
            {
                cantidad = recibir_linea(padre, &entrada_padre, linea);
                if (cantidad == 2) break;
                if (cantidad <= 0) { ipc_abierto = 0; hacia_padre.usados = 0; fin = time(NULL); break; }
                snprintf(mensaje, sizeof(mensaje), "%s\n", linea);
                if (!hacia_cliente.usados) progreso = time(NULL);
                if (encolar(&hacia_cliente, mensaje) == -1) goto cerrar;
            }
        if (tcp_abierto && ipc_abierto && FD_ISSET(cliente, &lectura))
            for (n = 0; n < 32; n++)
            {
                cantidad = recibir_linea(cliente, &entrada_cliente, linea);
                if (cantidad == 2) break;
                if (cantidad <= 0 || strlen(linea) > TAM_LINEA - 9)
                {
                    tcp_abierto = 0; hacia_cliente.usados = 0; fin = time(NULL);
                    snprintf(mensaje, sizeof(mensaje), "DESCONECTADO %d\n", id);
                    if (encolar(&hacia_padre, mensaje) == -1) goto cerrar;
                    break;
                }
                /* El cliente no puede falsificar avisos IPC: todo lleva ACCION. */
                snprintf(mensaje, sizeof(mensaje), "ACCION %s\n", linea);
                if (!hacia_padre.usados) progreso = time(NULL);
                if (encolar(&hacia_padre, mensaje) == -1) goto cerrar;
            }
        pendientes = hacia_cliente.usados + hacia_padre.usados;
        if (tcp_abierto && FD_ISSET(cliente, &escritura) && vaciar_salida(cliente, &hacia_cliente) == -1) break;
        if (ipc_abierto && FD_ISSET(padre, &escritura) && vaciar_salida(padre, &hacia_padre) == -1)
        { ipc_abierto = 0; hacia_padre.usados = 0; fin = time(NULL); }
        if (hacia_cliente.usados + hacia_padre.usados < pendientes) progreso = time(NULL);
    }
cerrar:
    close(cliente); close(padre);
}

static void aceptar_cliente(int escucha)
{
    int cliente, canal[2], i, id = -1;
    pid_t pid;
    struct sockaddr_in direccion;
    socklen_t tam = sizeof(direccion);
    cliente = accept(escucha, (struct sockaddr *)&direccion, &tam);
    if (cliente == -1)
    { if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) perror("accept"); return; }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, canal) == -1)
    { perror("socketpair"); close(cliente); return; }
    if (cliente >= FD_SETSIZE || canal[0] >= FD_SETSIZE || canal[1] >= FD_SETSIZE)
    { close(cliente); close(canal[0]); close(canal[1]); return; }
    for (i = 0; i < MAX_JUGADORES; i++)
        if (canales[i].descriptor < 0) { id = i; break; }
    fflush(stdout);
    pid = fork();
    if (pid == -1)
    { perror("fork"); close(cliente); close(canal[0]); close(canal[1]); return; }
    if (pid == 0)
    {
        close(escucha); close(canal[0]);
        for (i = 0; i < MAX_JUGADORES; i++)
            if (canales[i].descriptor >= 0) close(canales[i].descriptor);
        signal(SIGCHLD, SIG_DFL);
        puente_cliente(cliente, canal[1], id);
        _exit(EXIT_SUCCESS);
    }
    close(cliente); close(canal[1]);
    printf("Master: conexion desde %s; hijo %ld; lugar %d.\n", inet_ntoa(direccion.sin_addr), (long)pid, id);
    if (id == -1)
    {
        /* El sexto cliente tambien es atendido por un hijo, sin ocupar jugador. */
        (void)enviar_todo(canal[0], "SERVIDOR_LLENO\n");
        close(canal[0]);
        return;
    }
    memset(&canales[id], 0, sizeof(canales[id]));
    canales[id].descriptor = canal[0];
    canales[id].ingreso = canales[id].ultimo_envio = time(NULL);
    FD_SET(canal[0], &conjunto_master);
    enviar_mensaje(id, "CONECTADO Envia UNIRSE nombre.\n");
}

int main(int argc, char **argv)
{
    int escucha, opcion = 1, puerto = PUERTO_PREDETERMINADO, i, maximo, res, n, cantidad;
    struct sockaddr_in direccion;
    struct sigaction accion;
    struct timeval espera;
    fd_set lectura, escritura;
    char linea[TAM_LINEA];
    size_t pendientes;
    unsigned int semilla = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    if (argc > 3 || (argc >= 2 && (puerto = puerto_valido(argv[1])) == -1))
    { fprintf(stderr, "Uso: %s [puerto [semilla_de_prueba]]\n", argv[0]); return EXIT_FAILURE; }
    if (argc == 3)
    {
        char *fin;
        unsigned long valor;
        errno = 0; valor = strtoul(argv[2], &fin, 10);
        if (errno || !*argv[2] || *fin || argv[2][0] == '-' || valor > 0xffffffffUL)
        { fprintf(stderr, "Semilla invalida.\n"); return EXIT_FAILURE; }
        semilla = (unsigned int)valor;
    }
    srand(semilla);
    inicializar_juego();
    for (i = 0; i < MAX_JUGADORES; i++) canales[i].descriptor = -1;
    memset(&accion, 0, sizeof(accion));
    sigemptyset(&accion.sa_mask);
    accion.sa_handler = recoger_hijos; accion.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &accion, NULL) == -1) { perror("sigaction"); return EXIT_FAILURE; }
    accion.sa_handler = detener; accion.sa_flags = 0;
    if (sigaction(SIGINT, &accion, NULL) == -1 || sigaction(SIGTERM, &accion, NULL) == -1)
    { perror("sigaction"); return EXIT_FAILURE; }
    escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (escucha == -1) { perror("socket"); return EXIT_FAILURE; }
    if (escucha >= FD_SETSIZE || setsockopt(escucha, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion)) == -1 ||
        fcntl(escucha, F_SETFL, O_NONBLOCK) == -1)
    { perror("Configurar escucha"); close(escucha); return EXIT_FAILURE; }
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((unsigned short)puerto);
    direccion.sin_addr.s_addr = INADDR_ANY;
    if (bind(escucha, (struct sockaddr *)&direccion, sizeof(direccion)) == -1 || listen(escucha, COLA_CONEXIONES) == -1)
    { perror("bind/listen"); close(escucha); return EXIT_FAILURE; }
    FD_ZERO(&conjunto_master); FD_SET(escucha, &conjunto_master);
    printf("Blackjack - Master PID %ld - TCP %d - hasta %d jugadores\n", (long)getpid(), puerto, MAX_JUGADORES);
    printf("Ctrl+C para finalizar. Semilla: %u\n", semilla); fflush(stdout);

    while (!terminar)
    {
        lectura = conjunto_master; FD_ZERO(&escritura); maximo = escucha;
        for (i = 0; i < MAX_JUGADORES; i++)
        {
            CanalHijo *c = &canales[i];
            if (c->descriptor < 0) continue;
            if (c->cerrando) FD_CLR(c->descriptor, &lectura);
            if (c->salida.usados) FD_SET(c->descriptor, &escritura);
            if (c->descriptor > maximo) maximo = c->descriptor;
        }
        espera.tv_sec = 1; espera.tv_usec = 0;
        res = select(maximo + 1, &lectura, &escritura, NULL, &espera);
        if (res == -1) { if (errno == EINTR) continue; perror("select master"); break; }
        /* Primero los canales existentes; despues accept(), evitando reutilizar un fd de esta lectura. */
        for (i = 0; i < MAX_JUGADORES; i++)
        {
            CanalHijo *c = &canales[i];
            if (c->descriptor < 0) continue;
            if (FD_ISSET(c->descriptor, &escritura))
            {
                pendientes = c->salida.usados;
                if (vaciar_salida(c->descriptor, &c->salida) == -1) c->fallo = 1;
                if (c->salida.usados < pendientes) c->ultimo_envio = time(NULL);
            }
            if (!c->cerrando && !c->fallo && FD_ISSET(c->descriptor, &lectura))
                for (n = 0; n < 32 && !c->cerrando; n++)
                {
                    cantidad = recibir_linea(c->descriptor, &c->receptor, linea);
                    if (cantidad == 2) break;
                    if (cantidad <= 0) { c->fallo = 1; break; }
                    if (!strncmp(linea, "ACCION ", 7)) procesar_accion(i, linea + 7);
                    else { c->fallo = 1; break; } /* DESCONECTADO o IPC invalido. */
                }
            if (!c->cerrando && !jugadores[i].conectado && time(NULL) - c->ingreso >= 30)
                despedir(i, "ERROR Tiempo para registrarse agotado.\n");
            if (c->fallo || (c->salida.usados && time(NULL) - c->ultimo_envio >= 10) ||
                (c->cerrando && (!c->salida.usados || time(NULL) - c->ingreso >= 3))) cerrar_canal(i);
        }
        if (FD_ISSET(escucha, &lectura)) aceptar_cliente(escucha);
    }
    close(escucha);
    for (i = 0; i < MAX_JUGADORES; i++)
        if (canales[i].descriptor >= 0)
        {
            (void)encolar(&canales[i].salida, "SERVIDOR_CERRADO\n");
            (void)vaciar_salida(canales[i].descriptor, &canales[i].salida);
            close(canales[i].descriptor);
        }
    /* Al cerrar IPC los puentes salen; su vaciado final tiene limite de 3 s. */
    while (waitpid(-1, NULL, 0) != -1 || errno == EINTR) { }
    printf("Master finalizado.\n");
    return EXIT_SUCCESS;
}
