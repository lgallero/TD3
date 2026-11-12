#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t recibida;

/* Operación: registra SIGUSR1. Entrada: señal. Salida: bandera recibida. */
static void recibir_usr1(int senal)
{
    (void)senal;
    recibida = 1;
}

/* Operación: muestra el PID y espera SIGUSR1.
 * Entrada: máscara de señales para la espera. Salida: 0 si recibió SIGUSR1 o 1 si falla.
 */
static int ejecutar_hijo(sigset_t mascara)
{
    struct sigaction accion = {0};
    accion.sa_handler = recibir_usr1;
    sigemptyset(&accion.sa_mask);
    if (sigaction(SIGUSR1, &accion, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("Soy el proceso hijo %ld\n", (long)getpid());
    fflush(stdout);
    sigdelset(&mascara, SIGUSR1);

    while (!recibida)
        sigsuspend(&mascara);

    printf("Hijo %ld: recibí SIGUSR1.\n", (long)getpid());
    return 0;
}

/* Operación: espera una S en stdin y se la comunica al hijo con SIGUSR1.
 * Entrada: caracteres por stdin. Salida: mensajes en consola y código de salida.
 */
int main(void)
{
    sigset_t bloquear, anterior;
    sigemptyset(&bloquear);
    sigaddset(&bloquear, SIGUSR1);
    /* El hijo hereda el bloqueo, incluso si el padre lee la S enseguida. */
    if (sigprocmask(SIG_BLOCK, &bloquear, &anterior) == -1) {
        perror("sigprocmask");
        return 1;
    }

    pid_t hijo = fork();
    if (hijo == -1) {
        perror("fork");
        return 1;
    }
    if (hijo == 0)
        return ejecutar_hijo(anterior);

    printf("Padre %ld. Ingresá S para enviar SIGUSR1.\n", (long)getpid());
    fflush(stdout);

    int tecla;
    do {
        tecla = getchar();
    } while (tecla != 'S' && tecla != EOF);

    int resultado = 0;
    if (tecla == EOF && ferror(stdin)) {
        perror("stdin");
        resultado = 1;
    }
    int senal = SIGUSR1;
    if (tecla == EOF)
        senal = SIGKILL;
    if (kill(hijo, senal) == -1) {
        perror("kill");
        resultado = 1;
    }

    int estado;
    while (waitpid(hijo, &estado, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return 1;
        }
    }
    if (tecla == 'S' && (!WIFEXITED(estado) || WEXITSTATUS(estado) != 0))
        resultado = 1;
    return resultado;
}
