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
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define LISTENBUF 10
#define MYPORT 5000

int main (void)
{
  int sockfd, new_fd, sin_size;
  
  struct sockaddr_in my_addr;
  struct sockaddr_in their_addr;

  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  
  my_addr.sin_family 		= AF_INET;
  my_addr.sin_port 		= htons (MYPORT);
  my_addr.sin_addr.s_addr 	= INADDR_ANY;
  memset (&(my_addr.sin_zero),'\0',8);
  
  bind (sockfd, (struct sockaddr *) &my_addr, sizeof (struct sockaddr));
  
  listen (sockfd, LISTENBUF);
  
  while (1)
    {
    new_fd = accept (sockfd, (struct sockaddr *)&their_addr, &sin_size);
  
    printf ("Server: Connection from %s\n", inet_ntoa(their_addr.sin_addr));
  
    send (new_fd, "Hola!!\n",7,0);
    close (new_fd);
    sleep(1);
    }
    
}
  
  
  
















