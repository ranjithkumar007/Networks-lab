#include "transport-wrapper.h"
#include <iostream>
#include <cassert>
#define BUFSIZE 1024
using namespace std;

int main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr,"usage: %s <hostname> <server_port> <my_port> <filename>\n", argv[0]);
		exit(0);
	}

	char *hostname;
    int portno, flen,myport;
    char *filename;
    char buf[BUFSIZE];
    hostname = argv[1];
    portno = atoi(argv[2]);
    myport = atoi(argv[3]);
	filename = argv[4];

	FILE* fp = fopen(filename, "r");
	if (fp == NULL) {
		cerr << ("Error opening the file\n");
		exit(1);
	}

    fseek(fp, 0L, SEEK_END);
    long long int filesize = ftell(fp);
    rewind(fp);

	init_rhnet(myport);
	int socketfd = app_connect(hostname, portno);

	int kk = 1;
	while ((flen = fread(buf, 1, BUFSIZE, fp)) > 0) {
		app_send(socketfd, buf, flen);
		filesize -= flen;
		cout << "sent " << flen << " bytes, remaining = " << filesize << " bytes\n";
	}
	bzero(buf, BUFSIZE);
	app_recv(socketfd, buf, 26);
	cout << "Echo from server = " << buf << endl;
	long long int tm = 2000000000;
	while(tm--);
	cout << "File sent successfully\n";
}
