/* Ejercicio 3.5: sender archivo.txt */
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

/* Para probar en dos computadoras, cambiar HOST por la IP del servidor. */
#define HOST "127.0.0.1"
#define PORT 5024
#define BLOQUE 50
#define INTERVALO 30

int main(int argc, char *argv[])
{
    int tcpfd, udpfd, numbytes, puerto, cantidad;
    unsigned short puerto_red;
    struct hostent *he;
    struct sockaddr_in their_addr;
    char buf[BLOQUE];
    FILE *archivo;

    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s archivo.txt\n", argv[0]);
        return 1;
    }

    /* Comprobamos que el archivo existe antes de conectar al servidor. */
    archivo = fopen(argv[1], "rb");
    if (archivo == NULL)
    {
        perror("fopen");
        return 1;
    }
    fclose(archivo);

    he = gethostbyname(HOST);
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

    close(tcpfd);
    archivo = fopen(argv[1], "rb");
    if (archivo == NULL)
    {
        perror("fopen");
        close(udpfd);
        return 1;
    }

    /* Primer bloque inmediato; luego, un bloque cada 30 segundos. */
    cantidad = fread(buf, 1, BLOQUE, archivo);
    while (cantidad > 0)
    {
        numbytes = sendto(udpfd, buf, cantidad, 0,
                          (struct sockaddr *)&their_addr, sizeof(their_addr));
        if (numbytes == -1)
        {
            perror("sendto");
            fclose(archivo);
            close(udpfd);
            return 1;
        }
        printf("Enviados %d bytes.\n", numbytes);
        fflush(stdout);

        /* Leemos antes de dormir para no demorar al terminar el archivo. */
        cantidad = fread(buf, 1, BLOQUE, archivo);
        if (cantidad > 0)
            sleep(INTERVALO);
    }

    if (ferror(archivo))
    {
        fprintf(stderr, "Error al leer el archivo.\n");
        fclose(archivo);
        close(udpfd);
        return 1;
    }
    fclose(archivo);
    close(udpfd);
    return 0;
}
