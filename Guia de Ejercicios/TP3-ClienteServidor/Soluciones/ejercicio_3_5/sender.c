/* Ejercicio 3.5: ./sender archivo.txt */
/* Lucas Gallero - 25632 - TD3 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define HOST "127.0.0.1"
#define TCP_PORT 5024
#define BLOQUE 50
#define INTERVALO 30

int main(int argc, char *argv[])
{
    int tcp_sock, udp_sock, puerto, cantidad, recibidos = 0, primer_bloque = 1;
    char mensaje_puerto[16], buffer[BLOQUE];
    struct hostent *host;
    struct sockaddr_in direccion;
    FILE *archivo;

    if (argc != 2) {  
        fprintf(stderr, "Uso: %s archivo.txt\n", argv[0]);
        return 1;
    }
    
    archivo = fopen(argv[1], "r");
    if (archivo == NULL) {
        perror("fopen");
        return 1;
    }
    host = gethostbyname(HOST);
    if (host == NULL) {
        herror("gethostbyname");
        return 1;
    }
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock == -1) {
        perror("socket TCP");
        return 1;
    }
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(TCP_PORT);
    direccion.sin_addr = *((struct in_addr *)host->h_addr_list[0]);

    if (connect(tcp_sock, (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
        perror("connect");
        return 1;
    }
    
    while (recibidos < (int)sizeof(mensaje_puerto) - 1) {
        cantidad = recv(tcp_sock, mensaje_puerto + recibidos,
                        sizeof(mensaje_puerto) - 1 - recibidos, 0);
        if (cantidad == -1) {
            perror("recv puerto");
            return 1;
        }
        if (cantidad == 0)
            break;
        recibidos += cantidad;
    }
    close(tcp_sock);
    if (recibidos == 0) {
        fprintf(stderr, "El servidor no asigno un puerto: ocupado o con un error.\n");
        return 1;
    }
    mensaje_puerto[recibidos] = '\0';
    puerto = atoi(mensaje_puerto);
    if (puerto < 10000 || puerto > 65535) {
        fprintf(stderr, "Puerto UDP invalido.\n");
        return 1;
    }
    printf("Puerto UDP asignado: %d.\n", puerto);

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        perror("socket UDP");
        return 1;
    }
    direccion.sin_port = htons(puerto);

    while ((cantidad = fread(buffer, 1, BLOQUE, archivo)) > 0) {

        if (!primer_bloque)
            sleep(INTERVALO);
        primer_bloque = 0;

        if (sendto(udp_sock, buffer, cantidad, 0,
                   (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
            perror("sendto");
            return 1;
        }
        printf("Enviados %d bytes.\n", cantidad);
        fflush(stdout);
    }
    if (ferror(archivo)) {
        perror("lectura del archivo");
        return 1;
    }
    fclose(archivo);
    close(udp_sock);
    printf("Archivo enviado.\n");
    return 0;
}
