/* Ejercicio 3.1. Este mismo cliente sirve para el ejercicio 3.3. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 8024
#define MAXDATASIZE 100

int main(int argc, char *argv[])
{
    int sockfd, numbytes, recibidos;
    char buf[MAXDATASIZE];
    struct hostent *he;
    struct sockaddr_in their_addr;

    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s hostname\n", argv[0]);
        return 1;
    }

    he = gethostbyname(argv[1]);
    if (he == NULL)
    {
        herror("gethostbyname");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }

    memset(&their_addr, 0, sizeof(their_addr));
    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(PORT);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr_list[0]);

    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(their_addr)) == -1)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }
    printf("Conexion TCP establecida. Esperando el mensaje...\n");
    fflush(stdout);

    /* Leemos hasta el salto de linea, aunque TCP divida el mensaje. */
    recibidos = 0;
    while (recibidos < MAXDATASIZE - 1)
    {
        numbytes = recv(sockfd, buf + recibidos, 1, 0);
        if (numbytes <= 0)
        {
            if (numbytes == -1)
                perror("recv");
            else
                fprintf(stderr, "El servidor cerro sin completar el mensaje.\n");
            close(sockfd);
            return 1;
        }
        recibidos++;
        if (buf[recibidos - 1] == '\n')
            break;
    }
    buf[recibidos] = '\0';
    printf("Recibido: %s", buf);

    close(sockfd);
    return 0;
}
