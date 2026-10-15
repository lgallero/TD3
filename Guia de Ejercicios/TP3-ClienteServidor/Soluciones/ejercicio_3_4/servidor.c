/* Ejercicio 3.4: TCP para el puerto y FIN; UDP para los datos. */
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
#include <signal.h>

#define MYPORT 3456
#define BACKLOG 10
#define PUERTO_INICIAL 10000
#define MAXDATASIZE 1024

/* Esta funcion la ejecuta cada hijo, con su propia conexion TCP. */
int atender_cliente(int new_fd)
{
    int udpfd, puerto, mayor_fd, resultado, numbytes, recibidos;
    unsigned short puerto_red;
    struct sockaddr_in udp_addr;
    fd_set lectura;
    char buf[MAXDATASIZE];
    char orden[4];
    char nombre[80];
    FILE *archivo;

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

    snprintf(nombre, sizeof(nombre), "hijo_%ld.txt", (long)getpid());
    archivo = fopen(nombre, "wb");
    if (archivo == NULL)
    {
        perror("fopen");
        close(udpfd);
        close(new_fd);
        return 1;
    }
    printf("Hijo %ld: archivo %s.\n", (long)getpid(), nombre);

    mayor_fd = udpfd;
    if (new_fd > mayor_fd)
        mayor_fd = new_fd;
    recibidos = 0;

    while (1)
    {
        /* Esperamos datos UDP o una orden por TCP. */
        FD_ZERO(&lectura);
        FD_SET(udpfd, &lectura);
        FD_SET(new_fd, &lectura);
        resultado = select(mayor_fd + 1, &lectura, NULL, NULL, NULL);
        if (resultado == -1)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            fclose(archivo);
            close(udpfd);
            close(new_fd);
            return 1;
        }

        if (FD_ISSET(udpfd, &lectura))
        {
            numbytes = recvfrom(udpfd, buf, sizeof(buf), 0, NULL, NULL);
            if (numbytes == -1)
            {
                perror("recvfrom");
                continue;
            }
            /* Escribimos exactamente los bytes recibidos. */
            if (fwrite(buf, 1, numbytes, archivo) != (size_t)numbytes)
            {
                perror("fwrite");
                fclose(archivo);
                close(udpfd);
                close(new_fd);
                return 1;
            }
            if (fflush(archivo) == EOF)
            {
                perror("fflush");
                fclose(archivo);
                close(udpfd);
                close(new_fd);
                return 1;
            }
            printf("Hijo %ld: guardo %d bytes.\n", (long)getpid(), numbytes);
        }

        if (FD_ISSET(new_fd, &lectura))
        {
            /* FIN tiene 3 bytes y puede llegar dividido en varias lecturas. */
            numbytes = recv(new_fd, orden + recibidos, 3 - recibidos, 0);
            if (numbytes <= 0)
            {
                if (numbytes == -1)
                    perror("recv TCP");
                else
                    printf("Hijo %ld: el cliente cerro TCP.\n", (long)getpid());
                break;
            }
            recibidos = recibidos + numbytes;
            if (recibidos == 3)
            {
                orden[3] = '\0';
                if (strcmp(orden, "FIN") == 0)
                {
                    printf("Hijo %ld: recibio FIN.\n", (long)getpid());
                    break;
                }
                /* La unica orden prevista por este protocolo es FIN. */
                fprintf(stderr, "Orden TCP desconocida: %s\n", orden);
                recibidos = 0;
            }
        }
    }

    fclose(archivo);
    close(udpfd);
    close(new_fd);
    return 0;
}

int main(void)
{
    int sockfd, new_fd, opcion;
    socklen_t sin_size;
    struct sockaddr_in my_addr, their_addr;
    pid_t pid;

    setbuf(stdout, NULL);
    signal(SIGCHLD, SIG_IGN);

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
        sin_size = sizeof(their_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
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
        close(new_fd);
    }
}
