#ifndef _RHNET_TRANSPORT_WRAPPER_H
#define _RHNET_TRANSPORT_WRAPPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/wait.h>
#include <signal.h>
#include <queue>
#include <deque>

#define MAX_SBUFFER_SIZE 10240 /* sender buffer size */
#define MAX_RBUFFER_SIZE 10240 /* receiver buffer size */
#define MAX_DATA_SIZE 1024

#define INIT_SWND_SIZE 5  /* in MSS */
#define INIT_CWND_SIZE 10 /* RFC6928 */
#define INIT_SSTHRESH 256

#define INIT_SEQ_NUM 0

#define MAX_SWND 32767 
#define MAX_RECV_SLEEPS 3
// #define LAZY_RECV 1		/* uncomment for active receiver */

#define ACK_TIMEOUT 2 /* in sec */

// #define MAX_SYN_RETRIES 3
#define MAX_BACKLOGS 5	/* maximum size of backlog queue */

/* possible types of packets */
enum packet_type { 
	ACKP,
	DATAP,
	SYNP,
	SYN_ACKP
};

struct syn_packet {
	long long int isn; 	/* Initial Sequence Number */
};

struct ack_packet {
	long long int ack;
	int rwnd;	/* in bytes */
};

struct data_packet{
	char buf[MAX_DATA_SIZE + 1];
    long long int seq;		/* byte seq number */
	int len;
};

struct packet {
	packet_type type;
	union {
		struct ack_packet apkt;
		struct data_packet dpkt;
		struct syn_packet spkt;
	} content;
};

struct ssock {
	int sid;

	int swnd;
	int cwnd;
	int rwnd;
	int ssthresh;
	int cwnd_cnt;
	bool is_timeout;
	bool skip_timeout;
	long long int prev_ack;
	long long int curr_ack;
	int num_dupacks;
	long long int baseptr;
	long long int currptr;

	pthread_t rate_t;
	pthread_t timeout_t;

	std::queue<char> send_buffer;
	std::deque<struct packet> pkt_buffer;

	pthread_mutex_t mutex_swnd;
	pthread_mutex_t mutex_sb;
};

struct rsock {
	int rwnd;
	long long int last_read_seq;
	long long int prev_ack;

	std::deque<std::pair<char, bool>> recv_buffer;

	pthread_mutex_t mutex_rb;
	pthread_t recv_buff_t;
};

/* sender side */
ssize_t app_connect(const char* dest_addr, int port);
ssize_t app_send(int sess_id, const void* buf, int len);

/* receiver side */
ssize_t app_accept(char* src_addr, int *port);
ssize_t app_recv(int sess_id, void* buf, int len);

/* call this init function at the start of main
 */
ssize_t init_rhnet(int port, int backlog = 3); // ~eq to bind() + listen()

#endif // _RHNET_TRANSPORT_WRAPPER_H
