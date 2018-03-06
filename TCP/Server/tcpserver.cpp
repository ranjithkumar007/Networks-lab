/* 
 * tcpserver.cpp - A simple TCP client
 * usage: tcpserver <host> <port> <filename>
 */

#include <bits/stdc++.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;
#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
	unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
	unsigned short int sin_family; /* Address family */
	unsigned short int sin_port;   /* Port number */
	struct in_addr sin_addr;   /* IP address */
	unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
	char    *h_name;        /* official name of host */
	char    **h_aliases;    /* alias list */
	int     h_addrtype;     /* host address type */
	int     h_length;       /* length of address */
	char    **h_addr_list;  /* list of addresses */
}
#endif
#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(string msg) {
	perror(msg.c_str());
	exit(1);
}

int main(int argc, char const *argv[])
{
	int parentfd; /* parent socket */
	int childfd; /* child socket */
	int portno; /* port to listen on */
	unsigned int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char buf[BUFSIZE + 1]; /* message buffer */
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
		cerr << "usage: " << argv[0] << " <port>\n";
		exit(1);
	}
	portno = atoi(argv[1]);
	/* 
	 * socket: create the parent socket 
	 */
	parentfd = socket(AF_INET, SOCK_STREAM, 0);
	if (parentfd < 0) 
		error("ERROR opening socket");


	/* setsockopt: Handy debugging trick that lets 
	 * us rerun the server immediately after we kill it; 
	 * otherwise we have to wait about 20 secs. 
	 * Eliminates "ERROR on binding: Address already in use" error. 
	 */
	optval = 1;
	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
			(const void *)&optval , sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char *) &serveraddr, sizeof(serveraddr));

	/* this is an Internet address (IPv4)*/
	serveraddr.sin_family = AF_INET;

	/* let the system figure out our IP address */
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* this is the port we will listen on */
	serveraddr.sin_port = htons((unsigned short)portno);

	/* 
	 * bind: associate the parent socket with a port 
	 */
	if (bind(parentfd, (struct sockaddr *) &serveraddr, 
				sizeof(serveraddr)) < 0) 
		error("ERROR on binding");

	/* 
	 * listen: make this socket ready to accept connection requests 
	 */
	if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
		error("ERROR on listen");
	cout << "Server Running ....\n";

	/* 
	 * main loop: wait for a connection request, echo input line, 
	 * then close connection.
	 */
	clientlen = sizeof(clientaddr);
	while (1) {

		/* 
		 * accept: wait for a connection request 
		 */
		childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
		if (childfd < 0) 
			error("ERROR on accept");

		/* 
		 * determine who sent the message 
		 * inet_ntoa : internet host address in network byte order to a string in dotted-decimal notation.
		 */
		hostaddrp = inet_ntoa(clientaddr.sin_addr);
		if (hostaddrp == NULL)
			error("ERROR on inet_ntoa\n");
		cout << "server established connection with " << hostaddrp << endl;

		/* 
		 * read: read hello message containing filename and filesize from the client
		 */
		bzero(buf, BUFSIZE);
		n = read(childfd, buf, BUFSIZE);
		if (n < 0) 
			error("ERROR reading from socket");
		cout << "server received " << n << " bytes: " << buf << endl;

		/*
		 * split the given message into filename and filesize.
		 */
		strcpy(filename, strtok(buf, " "));
		int filesize = atoi(strtok(NULL, " "));
		cout << "filename : " << filename << ", filesize : " << filesize << "\n";

		/* 
		 * write: echo the input string back to the client as acknowledgement. 
		 */
		n = write(childfd, buf, strlen(buf));
		if (n < 0) 
			error("ERROR writing to socket");

		/*
		 *  open the file, create if doesn't exist and truncate it to zero bytes if exists.
		 */
		FILE *fp = fopen(filename, "w");
		if (fp == NULL) 
			error("creating a file error\n");
		bzero(buf, BUFSIZE);
		buf[BUFSIZE] = '\0';

		/*
		 *  read the message from the client in chunks of BUFSIZE and write it to the file. 
		 */
		curs = 0;
		while ((n = read(childfd, buf, BUFSIZE)) > 0) {
			curs += n;
			cout << "server received " << n << " bytes, remaining " << filesize - curs << " bytes\n";
			n = fwrite(buf, n, 1, fp);
			bzero(buf, BUFSIZE);
			if (curs >= filesize) 
				break;
		}
		fclose(fp);
		bzero(buf, BUFSIZE);

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
			cout << "Failed to run md5sum command\n";
			exit(1);
		}
		fscanf(pp, "%s", buf);
		pclose(pp);

		/*
		 *  write the md5sum of the file to the client. 
		 */
		n = write(childfd, buf, strlen(buf));
		if (n < 0) 
			error("ERROR writing to socket");

		/*
		 * Finally close this connection and wait for a new connection.
		 */
		close(childfd);
	}
	return 0;
}
