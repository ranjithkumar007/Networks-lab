/* 
 * tcpclient.cpp - A simple TCP client
 * usage: tcpclient <host> <port> <filename>
 */

#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
using namespace std;
#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(string msg) {
	perror(msg.c_str());
	exit(1);
}

int main(int argc, char **argv) {
	int sockfd, portno, n;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
	char buf[BUFSIZE + 1];
	int bytesRead;
	char *filename;
	int filesize;
	char command[100];
	char md5sum_output[200];

	/* check command line arguments */
	if (argc != 4) {
		cerr << "usage: "<<argv[0] <<" <hostname> <port> <filename>\n";
		exit(0);
	}
	hostname = argv[1];
	portno = atoi(argv[2]);

	// TCP : SOCK_STREAM
	/* socket: create the socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	/* gethostbyname: get the server's DNS entry */
	server = gethostbyname(hostname);
	if (server == NULL) {
		cerr << "ERROR, no such host as " << hostname << endl;
		exit(0);
	}

	/* build the server's Internet address */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
			(char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(portno);

	/* connect: create a connection with the server */
	if (connect(sockfd, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
		error("ERROR connecting");

	filename = argv[3];
	FILE* fp = fopen(filename, "r");
	if (fp == NULL)
		error("Error opening the file\n");

	/* find filesize */
	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);
	rewind(fp);

	/* prepare initial msg with filename and filesize */
	bzero(buf, BUFSIZE);
	sprintf(buf, "%s %d", filename, filesize);
	cout << buf << " " << strlen(buf) << endl;

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
	cout << "Actual MD5 value(from client) : " << md5sum_output << endl;
	pclose(pp);

	/* send the message containing filename and filesize to the server */
	n = write(sockfd, buf, strlen(buf));
	if (n < 0) 
		error("ERROR writing to socket");

	/* print the server's reply(Acknowledgement) */
	bzero(buf, BUFSIZE);
	n = read(sockfd, buf, BUFSIZE);
	if (n < 0) 
		error("ERROR reading from socket");
	cout << "Echo from server: " << buf << endl;

	bzero(buf, BUFSIZE);
	buf[BUFSIZE] = '\0';
	int curs = 0;

	/* read from the file and write it to the server */
	while ((bytesRead = fread(buf, 1, BUFSIZE, fp)) > 0) {
		curs += bytesRead;
		n = write(sockfd, buf, bytesRead);
		cout << "client sent " << bytesRead <<" bytes, remaining " << filesize - curs << " bytes\n";
		if (n < 0) 
			error("ERROR writing to socket");
		bzero(buf, BUFSIZE);
	}
	fclose(fp);

	/* get md5sum of the file received by the server */
	bzero(buf, BUFSIZE);
	n = read(sockfd, buf, BUFSIZE);
	if (n < 0) 
		error("ERROR reading from socket");
	cout << "Echo from server: "<< buf << endl;

	if (strcmp(buf, md5sum_output) == 0)
		cout << "MD5 Matched\n";
	else
		cout << "MD5 not Matched\n";

	close(sockfd);
	return 0;
}
