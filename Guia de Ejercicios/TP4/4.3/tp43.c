#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t crear;
static volatile sig_atomic_t recoger;

/* Operación: registra una solicitud de hijo. Entrada: SIGUSR1.
 * Salida: bandera crear para que el padre haga fork.
 */
static void pedir_hijo(int senal)
{
    (void)senal;
    crear = 1;
}

/* Operación: registra la salida de un hijo. Entrada: SIGCHLD.
 * Salida: bandera recoger para que el padre llame a waitpid.
 */
static void avisar_salida(int senal)
{
    (void)senal;
    recoger = 1;
}

/* Operación: atiende Ctrl+C. Entrada: SIGINT. Salida: ninguna. */
static void atender_ctrl_c(int senal)
{
    (void)senal;
}

/* Operación: informa sus datos cada 10 segundos. Entrada: copia de Nchilds.
 * Salida: mensajes en consola mientras el proceso esté vivo.
 */
static void informar(int Nchilds)
{
    /* El hijo conserva el bloqueo de señales que heredó de fork. */
    while (1) {
        printf("Soy el proceso %ld. Mi padre es el proceso %ld. "
               "Mi GroupId es %lu. Mi copia de la variable Nchilds vale %d.\n",
               (long)getpid(), (long)getppid(), (unsigned long)getgid(), Nchilds);
        fflush(stdout);
        sleep(10);
    }
}

/* Operación: administra la creación y recolección de hijos por señales.
 * Entrada: máximo de hijos simultáneos por línea de comandos.
 * Salida: PID del padre y de cada hijo. Devuelve 1 si falla el inicio.
 */
int main(int argc, char **argv)
{
    int Nchilds = 0;
    char *fin;

    if (argc != 2) {
        fprintf(stderr, "Uso: %s maximo_de_hijos\n", argv[0]);
        return 1;
    }
    errno = 0;
    long maximo = strtol(argv[1], &fin, 10);
    if (errno == ERANGE || fin == argv[1] || *fin != '\0' ||
        maximo < 1 || maximo > INT_MAX) {
        fprintf(stderr, "El máximo debe ser un entero positivo.\n");
        return 1;
    }

    sigset_t bloquear, espera;
    /* SIGKILL y SIGSTOP siempre quedan fuera del bloqueo. */
    sigfillset(&bloquear);
    if (sigprocmask(SIG_SETMASK, &bloquear, NULL) == -1) {
        perror("sigprocmask");
        return 1;
    }
    espera = bloquear;
    sigdelset(&espera, SIGUSR1);
    sigdelset(&espera, SIGCHLD);
    sigdelset(&espera, SIGINT);

    struct sigaction accion = {0};
    sigemptyset(&accion.sa_mask);
    accion.sa_handler = pedir_hijo;
    if (sigaction(SIGUSR1, &accion, NULL) == -1) {
        perror("sigaction SIGUSR1");
        return 1;
    }
    accion.sa_handler = atender_ctrl_c;
    if (sigaction(SIGINT, &accion, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }
    accion.sa_handler = avisar_salida;
    accion.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &accion, NULL) == -1) {
        perror("sigaction SIGCHLD");
        return 1;
    }

    printf("Padre %ld. Máximo de hijos: %ld.\n", (long)getpid(), maximo);
    fflush(stdout);

    while (1) {
        /* sigsuspend habilita las señales y duerme en una sola operación. */
        while (!crear && !recoger)
            sigsuspend(&espera);

        if (recoger || crear) {
            recoger = 0;
            /* Una señal SIGCHLD puede corresponder a varias salidas. */
            while (waitpid(-1, NULL, WNOHANG) > 0)
                Nchilds--;
        }

        if (crear) {
            crear = 0;
            if (Nchilds >= maximo) {
                printf("Ya hay %d hijos. Se alcanzó el máximo.\n", Nchilds);
                fflush(stdout);
                continue;
            }

            Nchilds++;
            pid_t pid = fork();
            if (pid == -1) {
                Nchilds--;
                perror("fork");
            }
            if (pid == 0) {
                informar(Nchilds);
                _exit(0);
            }
        }
    }
}
