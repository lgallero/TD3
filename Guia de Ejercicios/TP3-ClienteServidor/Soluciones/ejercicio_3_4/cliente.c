/* Cliente auxiliar del ejercicio 3.4: mensajes por teclado y FIN por TCP. */
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

#define PORT 3456
#define MAXDATASIZE 1024

int main(int argc, char *argv[])
{
    int tcpfd, udpfd, numbytes, puerto, enviados;
    unsigned short puerto_red;
    struct hostent *he;
    struct sockaddr_in their_addr;
    char buf[MAXDATASIZE];

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

    tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpfd == -1)
    {
        perror("socket TCP");
        return 1;
    }

    memset(&their_addr, 0, sizeof(their_addr));
    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(PORT);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr_list[0]);

    if (connect(tcpfd, (struct sockaddr *)&their_addr, sizeof(their_addr)) == -1)
    {
        perror("connect");
        close(tcpfd);
        return 1;
    }

    /* MSG_WAITALL espera los 2 bytes que representan el puerto. */
    numbytes = recv(tcpfd, &puerto_red, sizeof(puerto_red), MSG_WAITALL);
    if (numbytes != sizeof(puerto_red))
    {
        fprintf(stderr, "No se recibio el puerto UDP completo.\n");
        close(tcpfd);
        return 1;
    }
    puerto = ntohs(puerto_red);
    if (puerto == 0)
    {
        fprintf(stderr, "Servidor ocupado. Volver a intentar mas tarde.\n");
        close(tcpfd);
        return 1;
    }

    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpfd == -1)
    {
        perror("socket UDP");
        close(tcpfd);
        return 1;
    }
    their_addr.sin_port = htons(puerto);
    printf("Puerto UDP asignado: %d.\n", puerto);

    printf("Escriba mensajes. Para terminar, escriba FIN en una linea.\n");
    while (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        if (strcmp(buf, "FIN\n") == 0 || strcmp(buf, "FIN") == 0)
            break;
        numbytes = sendto(udpfd, buf, strlen(buf), 0,
                          (struct sockaddr *)&their_addr, sizeof(their_addr));
        if (numbytes == -1)
        {
            perror("sendto");
            close(udpfd);
            close(tcpfd);
            return 1;
        }
        printf("Enviados %d bytes por UDP.\n", numbytes);
    }

    enviados = 0;
    while (enviados < 3)
    {
        numbytes = send(tcpfd, "FIN" + enviados, 3 - enviados, MSG_NOSIGNAL);
        if (numbytes <= 0)
        {
            perror("send FIN");
            close(udpfd);
            close(tcpfd);
            return 1;
        }
        enviados = enviados + numbytes;
    }

    close(udpfd);
    close(tcpfd);
    return 0;
}
