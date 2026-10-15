/* Ejercicio 3.5: servidor concurrente de logs remotos. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/wait.h>

#define MYPORT 5024
#define BACKLOG 10
#define MAX_HIJOS 10
#define PUERTO_INICIAL 10000
#define BLOQUE 50
#define INTERVALO 30
#define LIMITE_HIJO 120
#define LIMITE_PADRE 60

int atender_cliente(int new_fd)
{
    int udpfd, puerto, resultado, numbytes, segundos_sin_datos;
    unsigned short puerto_red;
    struct sockaddr_in udp_addr;
    fd_set lectura;
    struct timeval tiempo;
    char buf[BLOQUE];

    /* Buscamos un puerto UDP libre: 10000, 10001, 10002, ... */
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd == -1)
    {
        perror("socket UDP");
        close(new_fd);
        return 1;
    }

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;

    for (puerto = PUERTO_INICIAL; puerto <= 65535; puerto++)
    {
        udp_addr.sin_port = htons(puerto);
        if (bind(udpfd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) == 0)
            break;
        if (errno != EADDRINUSE)
        {
            perror("bind UDP");
            close(udpfd);
            close(new_fd);
            return 1;
        }
    }
    if (puerto > 65535)
    {
        fprintf(stderr, "No hay un puerto UDP libre.\n");
        close(udpfd);
        close(new_fd);
        return 1;
    }

    /* Avisamos el puerto por TCP: 2 bytes, en orden de red. */
    puerto_red = htons(puerto);
    if (send(new_fd, &puerto_red, sizeof(puerto_red), MSG_NOSIGNAL)
        != sizeof(puerto_red))
    {
        perror("send puerto UDP");
        close(udpfd);
        close(new_fd);
        return 1;
    }
    printf("Hijo %ld: puerto UDP %d.\n", (long)getpid(), puerto);

    /* En este ejercicio TCP solo se usa para informar el puerto UDP. */
    close(new_fd);
    segundos_sin_datos = 0;

    while (segundos_sin_datos < LIMITE_HIJO)
    {
        /* select modifica estos valores: hay que cargarlos en cada vuelta. */
        FD_ZERO(&lectura);
        FD_SET(udpfd, &lectura);
        tiempo.tv_sec = INTERVALO;
        tiempo.tv_usec = 0;

        resultado = select(udpfd + 1, &lectura, NULL, NULL, &tiempo);
        if (resultado == -1)
        {
            if (errno == EINTR)
                continue;
            perror("select hijo");
            close(udpfd);
            return 1;
        }
        if (resultado == 0)
        {
            segundos_sin_datos = segundos_sin_datos + INTERVALO;
            printf("Hijo %ld: %d segundos sin datos.\n",
                   (long)getpid(), segundos_sin_datos);
        }
        else
        {
            numbytes = recvfrom(udpfd, buf, sizeof(buf), 0, NULL, NULL);
            if (numbytes == -1)
            {
                perror("recvfrom");
                continue;
            }
            /* Cada datagrama reinicia los dos minutos de inactividad. */
            segundos_sin_datos = 0;
            printf("Hijo %ld, UDP %d, %d bytes: ",
                   (long)getpid(), puerto, numbytes);
            fwrite(buf, 1, numbytes, stdout);
            printf("\n");
        }
    }

    printf("Hijo %ld: termina por inactividad.\n", (long)getpid());
    close(udpfd);
    return 0;
}

int main(void)
{
    int sockfd, new_fd, opcion, resultado, hijos, segundos_sin_hijos;
    unsigned short puerto_red;
    socklen_t sin_size;
    struct sockaddr_in my_addr, their_addr;
    fd_set lectura;
    struct timeval tiempo;
    pid_t pid;

    setbuf(stdout, NULL);
    hijos = 0;
    segundos_sin_hijos = 0;

    /* 1. Creamos el socket TCP del servidor. */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }

    /* Permite volver a ejecutar el servidor despues de cerrarlo. */
    opcion = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   &opcion, sizeof(opcion)) == -1)
    {
        perror("setsockopt");
        close(sockfd);
        return 1;
    }

    /* 2. Preparamos la direccion local y el puerto TCP. */
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1)
    {
        perror("bind");
        close(sockfd);
        return 1;
    }

    /* 3. Habilitamos la cola de conexiones pendientes. */
    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("Servidor esperando conexiones en TCP %d.\n", MYPORT);

    while (1)
    {
        /* WNOHANG recoge los hijos terminados sin bloquear al padre. */
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            hijos--;
            printf("Termino el hijo %ld. Hijos activos: %d.\n", (long)pid, hijos);
        }

        if (hijos > 0)
            segundos_sin_hijos = 0;

        /* Revisamos cada segundo si termino algun hijo. */
        FD_ZERO(&lectura);
        FD_SET(sockfd, &lectura);
        tiempo.tv_sec = 1;
        tiempo.tv_usec = 0;
        resultado = select(sockfd + 1, &lectura, NULL, NULL, &tiempo);
        if (resultado == -1)
        {
            if (errno == EINTR)
                continue;
            perror("select padre");
            close(sockfd);
            return 1;
        }

        if (resultado == 0)
        {
            if (hijos == 0)
                segundos_sin_hijos++;
            if (segundos_sin_hijos >= LIMITE_PADRE)
            {
                printf("Padre: termina tras un minuto sin hijos ni conexiones.\n");
                break;
            }
            continue;
        }

        /* Una conexion entrante reinicia la espera del padre. */
        segundos_sin_hijos = 0;
        sin_size = sizeof(their_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        /* Recogemos tambien los hijos que terminaron durante select. */
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
            hijos--;

        if (hijos >= MAX_HIJOS)
        {
            /* El puerto 0 indica que no hay lugar para otro cliente. */
            puerto_red = htons(0);
            if (send(new_fd, &puerto_red, sizeof(puerto_red), MSG_NOSIGNAL) == -1)
                perror("send servidor ocupado");
            printf("Servidor ocupado: ya hay %d hijos.\n", MAX_HIJOS);
            close(new_fd);
            continue;
        }

        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            close(new_fd);
            continue;
        }
        if (pid == 0)
        {
            close(sockfd);
            return atender_cliente(new_fd);
        }

        hijos++;
        printf("Se creo el hijo %ld. Hijos activos: %d.\n", (long)pid, hijos);
        close(new_fd);
    }

    close(sockfd);
    return 0;
}
