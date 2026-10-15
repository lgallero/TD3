/* Ejercicio 3.1: servidor TCP no concurrente. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MYPORT 8024
#define BACKLOG 10
#define DEMORA 60

int main(void)
{
    int sockfd, new_fd, opcion, enviados, numbytes;
    socklen_t sin_size;
    struct sockaddr_in my_addr, their_addr;
    const char mensaje[] = "Conexión aceptada\n";

    setbuf(stdout, NULL);

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

        /* TCP es un flujo: repetimos send si envio solo una parte. */
        enviados = 0;
        while (enviados < (int)strlen(mensaje))
        {
            numbytes = send(new_fd, mensaje + enviados,
                            strlen(mensaje) - enviados, MSG_NOSIGNAL);
            if (numbytes <= 0)
            {
                perror("send");
                break;
            }
            enviados = enviados + numbytes;
        }

        printf("Conexion atendida: %s. Demora de %d segundos.\n",
               inet_ntoa(their_addr.sin_addr), DEMORA);
        sleep(DEMORA);
        close(new_fd);
    }
}
