#include <stdio.h>
#include <netdb.h> // for gethostbyname
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <math.h>
#include <float.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define ICMP_LEN 500
#define ICMP_PAC_ID 15
#define IP_PAC_ID 3000
#define DATA_LEN sizeof(struct timeval)
#define PACK_LEN sizeof(struct iphdr)+sizeof(struct icmphdr)+sizeof(struct timeval)

char *hostname;
char *ip;
struct sockaddr_in serveraddr;
struct sockaddr_in from;
int icmp_pac_seq = 1;
int transmitted = 0;
int received = 0;
double maxi=0, mini= DBL_MAX;
double totaltime = 0,totaltime2 = 0,timetaken=0;
struct timeval starttime,endtime;

void set_ipheader(struct iphdr *iphd)
{
	iphd->ihl = sizeof(struct iphdr) / sizeof (uint32_t);
	iphd->tos = 0;
	iphd->id = htons(IP_PAC_ID);
	iphd->version = 4;
	iphd->tot_len = htons(sizeof( struct iphdr) + sizeof( struct icmphdr) + sizeof( struct timeval));
	iphd->frag_off = 0;
	iphd->ttl = 64;	
	iphd->protocol = 1;
	iphd->check = 0;
	iphd->saddr = INADDR_ANY;
	iphd->daddr =  serveraddr.sin_addr.s_addr;
}

void set_icmpheader(struct icmphdr *icmphd)
{

	icmphd->type = ICMP_ECHO;	// for echo request type=8 and code=0
	icmphd->code = 0;
	icmphd->un.echo.id = htons(ICMP_PAC_ID);
	icmphd->un.echo.sequence = htons(icmp_pac_seq);
	icmphd->checksum = 0;

}

unsigned short calculate_checksum(uint8_t *pBuffer, int nLen)
{	
	unsigned short nWord;
    unsigned int nSum = 0;
    int i;

    for (i = 0; i < nLen; i = i + 2)
    {
        nWord = ((pBuffer [i] << 8) & 0xFF00) + (pBuffer [i + 1] & 0xFF);
        nSum = nSum + (unsigned int)nWord;
    }

    while (nSum >> 16)
    {
        nSum = (nSum & 0xFFFF) + (nSum >> 16);
    }

    nSum = ~nSum;

    return ((unsigned short) nSum);
}

int checkvalidity(struct icmphdr *recvhdr , struct icmphdr *sendhdr)
{
	if (recvhdr->type != 0)
        return 0;
    if (recvhdr->code != 0)
        return 0;
    if (recvhdr->un.echo.id != sendhdr->un.echo.id)
        return 0;
    if (recvhdr->un.echo.sequence != sendhdr->un.echo.sequence)
        return 0;
    return 1;
}

void ping(int sfd)
{
	// packet building
	uint8_t buf[ICMP_LEN];
	bzero(buf,ICMP_LEN);

	// fill ip header in packet
	struct iphdr iphd;
	bzero(&iphd, sizeof(struct iphdr));
	set_ipheader(&iphd);

	// fill icmp header in packet
	struct icmphdr icmphd;
	bzero(&icmphd, sizeof(struct icmphdr));
	set_icmpheader(&icmphd);

	// put the timestamp
	struct timeval sendtime,recvtime,resulttime;
	gettimeofday(&sendtime, NULL);

	memcpy(buf, &iphd, sizeof(struct iphdr));
	memcpy(buf + sizeof(struct iphdr), &icmphd, sizeof(struct icmphdr));
	memcpy(buf + sizeof(struct iphdr) + sizeof(struct icmphdr), &sendtime, sizeof(struct timeval));


	// calculate the checksums
	iphd.check = htons(calculate_checksum(buf,sizeof(struct iphdr)));
	icmphd.checksum = htons(calculate_checksum(buf + sizeof(struct iphdr), sizeof(struct icmphdr) + sizeof(struct timeval)));
	
	// copying all the headers and data into buf (packet)
	memcpy(buf, &iphd, sizeof(struct iphdr));
	memcpy(buf + sizeof(struct iphdr), &icmphd, sizeof(struct icmphdr));
	memcpy(buf + sizeof(struct iphdr) + sizeof(struct icmphdr), &sendtime, sizeof(struct timeval));

	// send through socket
	int s = sendto(sfd, buf, sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(struct timeval), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	// printf("sent to %s\n",inet_ntoa(((struct sockaddr_in)serveraddr).sin_addr ));
	if(s < 0){
		printf("Error writing to socket\n");
		exit(1);
	}

	// printf("%d sent\n",s);

	transmitted++;

	// recv buffer
	uint8_t recv_buf[ICMP_LEN];
	bzero(recv_buf,ICMP_LEN);
	int l = sizeof(serveraddr);
	struct sockaddr_in clientaddr;
	// recv from socket
	int r = recvfrom(sfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&clientaddr, &l);
	// printf("received from %s\n",inet_ntoa(((struct sockaddr_in)clientaddr).sin_addr ));

	// printf("%d received\n",r);

	if(r < 0)
	{
		printf("Timeout\n");
	}
	else
	{
		gettimeofday(&recvtime,NULL);

		// seperate ip header, icmp header and time field

		struct iphdr *rec_iphd = (struct iphdr *)recv_buf;
		struct icmphdr *rec_icmphd = (struct icmphdr *)(recv_buf + sizeof(struct iphdr));
		struct timeval *des_stime = (struct timeval *)(recv_buf + sizeof(struct iphdr) + sizeof(struct icmphdr));

		// printf("%d code\n",rec_icmphd->code);
		// printf("%d type\n",rec_icmphd->type);

		// if type and id matches the requirements calculate the time elapsed
		if(checkvalidity(rec_icmphd,&icmphd))
		{
			received++;
			timersub(&recvtime, des_stime, &resulttime);
			double time_elapsed = (resulttime.tv_sec) * 1000 + ((double)(resulttime.tv_usec)) / 1000;

			printf("%d bytes from %s (%s): icmp_seq=%d ttl=%d time=%0.3f ms\n",r,hostname,ip,icmp_pac_seq,rec_iphd->ttl,time_elapsed);
			if(time_elapsed < mini)
				mini = time_elapsed;
			if(time_elapsed > maxi)
				maxi = time_elapsed;
			totaltime += time_elapsed;
			totaltime2 += time_elapsed*time_elapsed;
		}
		else if(rec_icmphd->type == 3)
		{
			printf("Destination unreachable\n");
		}
	}

	icmp_pac_seq++;
	
}

void print_statistics(int sig)
{
	gettimeofday(&endtime,NULL);
	timersub(&endtime,&starttime,&endtime);
	timetaken = (endtime.tv_sec) * 1000 + ((double)(endtime.tv_usec)) / 1000;
	
	printf("---------- %s ping statistics------------\n",hostname);
	printf("%d packets transmitted, %d packets received, ",transmitted,received);

	double percent_loss = ((double)(transmitted - received)/(double)transmitted)*100;
	printf("%0.3f%% packetloss, time %0.3f ms\n",percent_loss,timetaken);

	totaltime /= received;
	totaltime2 /= received;
	double mdev = sqrt(totaltime2 - totaltime*totaltime);

	printf("rtt min/avg/max/mdev = %0.3f/%0.3f/%0.3f/%0.3f ms\n",mini,totaltime,maxi,mdev);
	exit(1);
}

void finalstat(int sig)
{
	double percent_loss = ((double)(transmitted - received)/(double)transmitted)*100;
    printf("%d/%d packets, %0.3f%% loss, min/avg/max = %0.3f/%0.3f/%0.3f ms\n",received,transmitted,percent_loss,mini,totaltime/received,maxi);
}

int main(int argc, char **argv)
{
	struct hostent *server;

	if(argc != 3){
		printf("Usage : %s <hostname> <count>\n",argv[0]);
		exit(1);
	}
	int count = atoi(argv[2]);
	hostname = argv[1];

	// checking the count 
	if(count <= 0){
		printf("Count should be greater than zero\n");
		exit(1);
	}

	// dns lookup
	server = gethostbyname(hostname);
	if(server == NULL){
		printf("Error, no such host as %s\n",hostname);
		exit(1);
	}

	// getting ip address
	struct in_addr **addr_list;
	addr_list = (struct in_addr **)(server->h_addr_list);
	ip = inet_ntoa(*(addr_list[0]));

	// build the destination internet address
	bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    // serveraddr.sin_addr.s_addr = inet_addr(ip);

	// create a raw socket
	int sfd = socket(AF_INET ,SOCK_RAW ,IPPROTO_ICMP);
	if(sfd < 0){
		printf("Error while creating the socket\n");
		exit(1);
	}

	// add timeout to socket
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO,  &tv , sizeof(tv));

	// mention that ip header is self built
	int on=1;
	setsockopt(sfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(int));

	printf("Ping %s (%s) %lu(%lu) bytes of data\n",hostname,ip,DATA_LEN,PACK_LEN);

	gettimeofday(&starttime,NULL);

	signal(SIGINT,print_statistics);
	signal(SIGQUIT,finalstat);
	// ping 
	while(count--)
	{
		ping(sfd);
		sleep(1);
	}

	// final stats of ping
	print_statistics(0);
	return 0;
}