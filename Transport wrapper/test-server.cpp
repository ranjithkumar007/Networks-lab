#include "transport-wrapper.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <string>
#define BUFSIZE 1024
using namespace std;

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr,"usage: %s <port> <filesize(bytes)>\n", argv[0]);
		exit(0);
	}

	// char *hostname;
	char *src_addr;
    int portno, flen;
    int sport;

    portno = atoi(argv[1]);
    long long int filesize = atoll(argv[2]);
	init_rhnet(portno);
	
	char buf[BUFSIZE + 1];
	int childfd = app_accept(src_addr, &sport);
	long long int total = filesize;
	cout << "Accepted new connection request\n";
	if (childfd != -1) {			
		FILE* outfile = fopen("server-output", "w");
		while (total > 0) {
			long long X = min((long long)BUFSIZE, total);
			app_recv(childfd, &buf, X);
			total -= X;
			fwrite(buf, 1, X, outfile);
			cout << "received " << X << " bytes, remaining = " << max(total, 0LL) << " bytes\n";
		}
		fclose(outfile);
	}
	bzero(buf, BUFSIZE);
	strcpy(buf, "File received successfully");
	app_send(childfd, buf, 26);
	long long int tm = 2000000000;
	while(tm--);
	cout << "File received successfully\n";
}
