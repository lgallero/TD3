/**
*********************************************************************************************************
*
*                       Ejemplo10
*                       GNU C Compiler
* Ejemplo de Sockets
*
* Filename      : ejemplo10.c
*
* UTN FRH - 2016
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT 5000
#define MAXDATASIZE 40

int main (int argc, char *argv[])
{
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  
  struct sockaddr_in their_addr;
  struct hostent *he;
  
  if (argc !=2) 
  {
     perror ("Cantidad de argumentos incorrecto\n");
     exit(1);
  }
  
  he=gethostbyname(argv[1]);

  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  
  their_addr.sin_family	= AF_INET;
  their_addr.sin_port	= htons (PORT);
  their_addr.sin_addr 	= *((struct in_addr*)he->h_addr);
  memset (&(their_addr.sin_zero),'\0',8);
  
  connect (sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
  
  printf ("Client: Connection to %s\n", inet_ntoa(their_addr.sin_addr));
  
    if((numbytes = recv (sockfd, buf,MAXDATASIZE-1,0))==-1)
    {
      perror("Recv");
      exit(1);
    }
    buf[numbytes]='\0';
    printf("Recibido: %s",buf);
    close (sockfd);
    
    return 0;
}
  
  
  
















