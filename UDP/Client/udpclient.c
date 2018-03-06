/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <math.h>

#define BUFSIZE 1024
#define MAX_RETRIES 10

// possible packet types - data or synchronisation packets
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
    exit(0);
}

void sendPacket(int sockfd, Packet sbuf, struct sockaddr_in serveraddr, int serverlen)
{
	int cnt = 0;
    while(1) {
    	cnt++;
	    int n = sendto(sockfd, &sbuf, sizeof(sbuf), 0, &serveraddr, serverlen);
	    if (n < 0) 
	      error("ERROR writing to socket");

	    /* print the server's reply */
	  	int ack;
	  	n = recvfrom(sockfd, (void *)&ack, sizeof(int), 0, &serveraddr, &serverlen);
	    printf("Expected ACK : %d, ACK from server : %d\n", sbuf.seq, ack);
	    if (ack == sbuf.seq && n >= 0) { // (case 6)
	    	return ;
	    } else printf("Correct ");
	    printf("Acknowledgement not received(timed out); retransmitting the packet\n"); // (case 4)
    	if (cnt == MAX_RETRIES) {
    		close(sockfd);
    		printf("Connection aborted !! file transfer not successful\n");
    		exit(1);
    	}
	}
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char *filename;
	int filesize;
	char command[100];
	char md5sum_output[200];

    /* check command line arguments */
    if (argc != 4) {
		fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
		exit(0);
	}
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 
	     &tv , sizeof(tv));

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    filename = argv[3];
	FILE* fp = fopen(filename, "r");
	if (fp == NULL)
		error("Error opening the file\n");

	/* find filesize */
	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);
	rewind(fp);

	int num_chunks = filesize / BUFSIZE + (filesize % BUFSIZE == 0 ? 0 : 1);

	/* prepare initial msg with filename, filesize and number of chunks*/
	bzero(buf, BUFSIZE);
	sprintf(buf, "%s %d %d", filename, filesize, num_chunks);
	printf("%s %d\n", buf, strlen(buf));

	/* get md5sum for the file */
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
	fscanf(pp, "%s", md5sum_output);
	printf("Actual MD5 value(from client) : %s\n", md5sum_output);
	pclose(pp);

	serverlen = sizeof(serveraddr);
	Packet sbuf;
	strncpy(sbuf.buf, buf, BUFSIZE);
	sbuf.seq = 0;
	sbuf.len = strlen(buf);
	sbuf.ty = SYNP;

	sendPacket(sockfd, sbuf, serveraddr, serverlen);

	int sn = 1;
	int flen = 0;

	Packet dbuf;
	while ((flen = fread(dbuf.buf, 1, BUFSIZE, fp)) > 0) {
		// preparing the DataPacket
		int j;
		dbuf.seq = sn;
		dbuf.len = flen;
		dbuf.ty = DATAP;

		sendPacket(sockfd, dbuf, serveraddr, serverlen);
		sn = 1 - sn;
	}

	/* get md5sum of the file received by the server */
    n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
	if (n < 0) 
		error("ERROR reading from socket");
	printf("Echo from server: %s\n", buf);

	if (strcmp(buf, md5sum_output) == 0)
		printf("MD5 Matched\n");
	else
		printf("MD5 not Matched\n");

	close(sockfd);
	
    return 0;
}
