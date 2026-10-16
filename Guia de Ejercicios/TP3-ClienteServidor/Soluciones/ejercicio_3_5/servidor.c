/* Ejercicio 3.5: servidor */
/* Lucas Gallero - 25632 - TD3 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>

#define TCP_PORT 5024
#define MAX_HIJOS 10
#define PUERTO_INICIAL 10000
#define BLOQUE 50
#define INTERVALO 30
#define LIMITE_HIJO 120
#define LIMITE_PADRE 60


int buscar_puerto_udp(int udp_sock)
{
    struct sockaddr_in direccion;
    int puerto;

    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;

    for (puerto = PUERTO_INICIAL; puerto <= 65535; puerto++) {
        direccion.sin_port = htons(puerto);
        if (bind(udp_sock, (struct sockaddr *)&direccion, sizeof(direccion)) == 0)
            return puerto;
        if (errno != EADDRINUSE)
            break;
    }
    return -1;
}


void atender_hijo(int tcp_sock)
{
    int udp_sock, puerto, resultado, cantidad, segundos_sin_datos = 0;
    char mensaje_puerto[16], buffer[BLOQUE + 1];
    fd_set lectura;
    struct timeval tiempo;

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        perror("socket UDP");
        exit(1);
    }
    puerto = buscar_puerto_udp(udp_sock);
    if (puerto == -1) {
        fprintf(stderr, "No se pudo reservar un puerto UDP.\n");
        exit(1);
    }

    snprintf(mensaje_puerto, sizeof(mensaje_puerto), "%d\n", puerto);
    if (send(tcp_sock, mensaje_puerto, strlen(mensaje_puerto), 0)
        != (ssize_t)strlen(mensaje_puerto)) {
        perror("send puerto");
        exit(1);
    }
    close(tcp_sock);
    printf("Hijo %ld: puerto UDP %d.\n", (long)getpid(), puerto);

    while (segundos_sin_datos < LIMITE_HIJO) {

        FD_ZERO(&lectura);
        FD_SET(udp_sock, &lectura);

        tiempo.tv_sec = INTERVALO;
        tiempo.tv_usec = 0;
        resultado = select(udp_sock + 1, &lectura, NULL, NULL, &tiempo);

        if (resultado == -1) {
            if (errno != EINTR) {
                perror("select hijo");
                exit(1);
            }
        } else if (resultado == 0) {
            segundos_sin_datos += INTERVALO;
            printf("Hijo UDP %d: %d segundos sin datos.\n", puerto, segundos_sin_datos);
        } else {
            cantidad = recvfrom(udp_sock, buffer, BLOQUE, 0, NULL, NULL);
            if (cantidad == -1) {
                perror("recvfrom");
            } else {
                buffer[cantidad] = '\0';
                printf("Hijo UDP %d: %s\n", puerto, buffer);
                segundos_sin_datos = 0;
            }
        }
    }
    printf("Hijo UDP %d: finaliza por inactividad.\n", puerto);
    close(udp_sock);
    exit(0);
}

int main(void)
{
    int tcp_sock, nuevo_sock, resultado, opcion = 1;
    int hijos = 0, segundos_sin_hijos = 0;
    pid_t pid;
    struct sockaddr_in direccion;
    fd_set lectura;
    struct timeval tiempo;

    setbuf(stdout, NULL);
    signal(SIGPIPE, SIG_IGN); 

    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock == -1) {
        perror("socket TCP");
        return 1;
    }
    if (setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion)) == -1) {
        perror("setsockopt");
        return 1;
    }
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(TCP_PORT);
    direccion.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_sock, (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
        perror("bind TCP");
        return 1;
    }
    if (listen(tcp_sock, MAX_HIJOS) == -1) {
        perror("listen");
        return 1;
    }
    printf("Servidor esperando conexiones en TCP %d.\n", TCP_PORT);

    while (1) {
      
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            hijos--;
            printf("Fin de un hijo. Hijos activos: %d.\n", hijos);
        }
        if (hijos > 0)
            segundos_sin_hijos = 0;

        FD_ZERO(&lectura);
        FD_SET(tcp_sock, &lectura);

        tiempo.tv_sec = 1; 
        tiempo.tv_usec = 0;
        resultado = select(tcp_sock + 1, &lectura, NULL, NULL, &tiempo);

        if (resultado == -1) {
            if (errno != EINTR) {
                perror("select padre");
                break;
            }
        } else if (resultado == 0) {
            if (hijos == 0)
                segundos_sin_hijos++;
            if (segundos_sin_hijos >= LIMITE_PADRE) {
                printf("Padre: Fin del programa tras un minuto sin hijos ni conexiones.\n");
                break;
            }
        } else {
            segundos_sin_hijos = 0;
            nuevo_sock = accept(tcp_sock, NULL, NULL);
            if (nuevo_sock == -1) {
                perror("accept");
            } else {
                while (waitpid(-1, NULL, WNOHANG) > 0)
                    hijos--;

                if (hijos >= MAX_HIJOS) {
                    printf("Servidor ocupado: ya hay %d hijos.\n", MAX_HIJOS);
                } else {
                    pid = fork();
                    if (pid == -1) {
                        perror("fork");
                    } else if (pid == 0) {
                        close(tcp_sock);
                        atender_hijo(nuevo_sock);
                    } else {
                        hijos++;
                        printf("Se creo un hijo. Hijos activos: %d.\n", hijos);
                    }
                }
                close(nuevo_sock);
            }
        }
    }
    close(tcp_sock);
    return 0;
}
