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

/* Operación: muestra el PID y espera 60 segundos.
 * Entrada: ninguna. Salida: mensaje en consola.
 */
static void informar(void)
{
    printf("Soy el proceso N°: %ld\n", (long)getpid());
    fflush(stdout);
    /* Las señales del padre siguen bloqueadas en el hijo mientras duerme. */
    unsigned int restantes = 60;
    while (restantes > 0)
        restantes = sleep(restantes);
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
    sigemptyset(&bloquear);
    sigaddset(&bloquear, SIGUSR1);
    sigaddset(&bloquear, SIGCHLD);
    sigaddset(&bloquear, SIGINT);
    if (sigprocmask(SIG_BLOCK, &bloquear, &espera) == -1) {
        perror("sigprocmask");
        return 1;
    }
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
                informar();
                _exit(0);
            }
        }
    }
}
