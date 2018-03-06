/* 
 * udpserver.c - A UDP echo server 
 * usage: udpserver <port_for_server>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#define BUFSIZE 1024

typedef enum { DATAP, SYNP} PType;

typedef struct {
  char buf[BUFSIZE];
  int seq;
  int len;
  PType ty;
} Packet;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  char command[100]; /* command to run md5sum on the file */
  char filename[100]; /* filename sent by client */
  int curs = 0; /* stores total number of bytes received until now */


  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  int isConnected = 0;
  int filesize, num_chunks, ack = -1;
  FILE *fp;
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    // bzero(buf, BUFSIZE);
    Packet pbuf;
    // strncpy(pbuf.buf, buf, BUFSIZE);

    n = recvfrom(sockfd, &pbuf, sizeof(pbuf), 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    // hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
    //     sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    // if (hostp == NULL)
    //   error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    // printf("server received datagram from %s (%s)\n", 
    //  hostp->h_name, hostaddrp);
    printf("server received datagram from %s \n", hostaddrp);
  

    int flg = 0;
    if (pbuf.ty != SYNP && ack == pbuf.seq) {
      flg = 1; // Duplicate packet (case 5).
    }
    
    ack = pbuf.seq;
    n = sendto(sockfd, (void *)&ack, sizeof(int), 0, 
         (struct sockaddr *) &clientaddr, clientlen);
    printf("Server sent ACK %d\n", ack);
    if (n < 0) 
      error("ERROR in sendto");

    if (flg) continue;
    if (pbuf.ty == SYNP) {
      printf("server received initial hello message as %s\n", pbuf.buf);
      strcpy(filename, strtok(pbuf.buf, " "));
      filesize = atoi(strtok(NULL, " "));
      num_chunks = atoi(strtok(NULL, " "));
      printf("filename : %s, filesize : %d, num_chunks : %d\n", filename, filesize, num_chunks);
      curs = 0;
      fp = fopen(filename, "w");
      if (fp == NULL) 
        error("creating a file error\n");
    } else {
      assert(pbuf.ty == DATAP);
      curs += pbuf.len;
      printf("server received %d bytes, remaining %d bytes\n", pbuf.len, filesize - curs);
      fwrite(pbuf.buf, pbuf.len, 1, fp);
      if (curs >= filesize) {
        fclose(fp);
        /*
         *  generate command to get md5sum of the file.
         */
        strcpy(command, "md5sum ");
        strcat(command, filename);

        /*
         *  popen : pipe stream to or from a process. 
         *  It opens a process by creating a pipe, forking, and invoking the shell.
         *  Output of the command is obtained from the pipe's read end. 
         */
        FILE* pp = popen(command, "r");
        if (pp == NULL) {
            printf("Failed to run md5sum command\n" );
            exit(1);
        }
        fscanf(pp, "%s", pbuf.buf);
        pclose(pp);

        /*
         *  write the md5sum of the file to the client. 
         */
        printf("md5sum on server side : %s\n", pbuf.buf);
        n = sendto(sockfd, pbuf.buf, BUFSIZE, 0, 
           (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
          error("ERROR in sendto");
      	curs = 0;
      }
    }
  }
}
