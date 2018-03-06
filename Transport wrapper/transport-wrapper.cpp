#include "transport-wrapper.h"
#include <cassert>
#include <pthread.h>
#include <string>
#include <limits.h>
#include <cstdlib>
#include <map>
#include <iostream>
#include <unistd.h>
#include <vector> 
using namespace std;

vector<struct sockaddr_in> sid_to_addr(1025);	// assuming all file desc <= 1024
map<int, struct ssock*> sid_to_ssock;
map<int, struct rsock*> sid_to_rsock; 

int parent_sockfd;
int num_conn = 0;

queue<int> backlogQ;
int max_backlogs;

pthread_mutex_t mutex_backlogQ = PTHREAD_MUTEX_INITIALIZER;
pthread_t main_t, recv_t;

void* rate_control(void *);
void* timeout_handler(void *);
void* receiver(void *);

static inline void error(string msg)
{
    perror(msg.c_str());
    exit(0);
}

void sig_handler(int signum)
{
	if (signum == SIGUSR1) {}
}

static inline struct packet create_packet(int seq) 
{
	struct packet pkt;
	pkt.type = SYNP;
	pkt.content.spkt.isn = seq;
	return pkt;
}

static inline struct packet create_packet(int seq, char* data, int len) 
{
	struct packet pkt;
	pkt.type = DATAP;
	pkt.content.dpkt.len = len;
	pkt.content.dpkt.seq = seq;
	for (int i = 0; i < len; i++)
		pkt.content.dpkt.buf[i] = data[i];
	return pkt;
}

static inline struct packet create_packet(int ack, int rwnd, bool is_synack = false) 
{
	struct packet pkt;
	if (is_synack)
		pkt.type = SYN_ACKP;
	else
		pkt.type = ACKP;
	pkt.content.apkt.ack = ack;
	pkt.content.apkt.rwnd = rwnd / MAX_DATA_SIZE; // convert to MSS
	return pkt;
}

static void init_ssock(int sid)
{
	struct ssock *ssck = new struct ssock;

	sid_to_ssock[sid] = ssck;
	ssck->rwnd = MAX_RBUFFER_SIZE / MAX_DATA_SIZE; /* TODO : get init_rwnd from syn_ack packet */
	ssck->swnd = INIT_SWND_SIZE; 
   	ssck->cwnd = INIT_CWND_SIZE;
   	ssck->ssthresh = INIT_SSTHRESH;

   	ssck->sid = sid;
   	ssck->cwnd_cnt = 0;
   	ssck->is_timeout = false;
   	ssck->prev_ack = -1;
   	ssck->curr_ack = -1;
   	ssck->num_dupacks = 0;
   	ssck->baseptr = -1;
   	ssck->currptr = 0;

	assert(pthread_create(&(ssck->rate_t), NULL, rate_control, (void *) ssck) == 0);
	ssck->mutex_sb = PTHREAD_MUTEX_INITIALIZER;
	ssck->mutex_swnd = PTHREAD_MUTEX_INITIALIZER;
	ssck->timeout_t = -1;
}

static inline void init_rsock(int sid)
{
	struct rsock *rsck = new struct rsock;
	sid_to_rsock[sid] = rsck;
	rsck->rwnd = MAX_RBUFFER_SIZE; 
   	rsck->last_read_seq = -1;
   	rsck->prev_ack = 0;
   	rsck->mutex_rb = PTHREAD_MUTEX_INITIALIZER;
}

static inline void udp_send(struct sockaddr_in addr, struct packet pkt)
{
	cout << "sending packet of type = " << pkt.type << endl;
    if (sendto(parent_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        error("ERROR writing to socket");
}

static struct packet udp_receive(struct sockaddr_in *addr)
{
	struct packet pkt;
    struct sockaddr_in saddr;
    size_t addr_len = sizeof(saddr);

	recvfrom(parent_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &saddr, (socklen_t *)&addr_len);
	cout << "received a packet of type = " << pkt.type << endl;		
	*addr = saddr;
	return pkt; 
}

static int add_conn(const char* dest_addr, int port)
{
	struct hostent* server;

	int sid = num_conn++;
	server = gethostbyname(dest_addr);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", dest_addr);
        exit(0);
    }

    struct sockaddr_in sconn;
    bzero((char*)&(sconn), sizeof((sconn)));
    (sconn).sin_family = AF_INET;
    bcopy((char*)server->h_addr,
        (char*)&(sconn).sin_addr.s_addr, server->h_length);
    (sconn).sin_port = htons(port);

    sid_to_addr[sid] = sconn;
    init_ssock(sid);
	init_rsock(sid);
    return sid;
}

static int add_conn(struct sockaddr_in addr)
{
	int sid = num_conn++;
	sid_to_addr[sid] = addr;
	init_ssock(sid);
	init_rsock(sid);
    return sid;
}

ssize_t app_connect(const char* dest_addr, int port)
{
	signal(SIGUSR1, sig_handler);
	int sid = add_conn(dest_addr, port);

	struct packet pkt = create_packet(INIT_SEQ_NUM);
	struct sockaddr_in dummy_addr;
	udp_send(sid_to_addr[sid], pkt);
	pause();
	return sid;
}

static ssize_t send_buffer_handle(struct ssock *ssck, const void* buf, int len)
{
	char* ptr = (char *) buf;
	
	while (1) {
		pthread_mutex_lock(&(ssck->mutex_sb));
		if ((ssck->send_buffer.size() + len) <= MAX_SBUFFER_SIZE) {
			break;
		} else {
			pthread_mutex_unlock(&(ssck->mutex_sb));
			pause();
		}
	}
	
	for (int i = 0; i < len; i++) {
		ssck->send_buffer.push(ptr[i]);
	}

	pthread_mutex_unlock(&(ssck->mutex_sb));
	pthread_kill(ssck->rate_t, SIGUSR1);
	return 0;
}

void create_rcv_socket(int port)
{
	struct sockaddr_in serveraddr;
	parent_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (parent_sockfd < 0) 
    	error("ERROR creating socket");

	int optval = 1;
	setsockopt(parent_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);

	if (bind(parent_sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
		error("ERROR on binding");
}

ssize_t init_rhnet(int port, int backlog)
{
	create_rcv_socket(port);
	main_t = pthread_self();
	max_backlogs = min(backlog, MAX_BACKLOGS);
	
	pthread_create(&recv_t, NULL, receiver, NULL);
}

ssize_t app_send(int sid, const void* buf, int len)
{
	signal(SIGUSR1, sig_handler);
	auto iter = sid_to_ssock.find(sid);
	assert(iter != sid_to_ssock.end());
	ssize_t ret = send_buffer_handle(iter->second, buf, len);
	return ret;
}

static char* get_data(struct ssock *ssck, int *len)
{
	while (1) {
		pthread_mutex_lock(&(ssck->mutex_sb));
		if ((ssck->send_buffer).empty()) {
			pthread_mutex_unlock(&(ssck->mutex_sb));
			pause();
		} else break;
	}
	
	int sz = min((int)((ssck->send_buffer).size()), MAX_DATA_SIZE);
	char* ptr = new char[sz + 1];
	int i;
	for (i = 0; i < sz; i++) {
		ptr[i] = (ssck->send_buffer).front();
		(ssck->send_buffer).pop();
	}
	*len = sz;
	pthread_mutex_unlock(&(ssck->mutex_sb));
	pthread_kill(main_t, SIGUSR1);
	return ptr;
}

void* timeout_handler(void * ptr)
{
	struct ssock *ssck = (struct ssock *) ptr;
	if (sleep(ACK_TIMEOUT) == 0) { /* use nanospleep? */
		pthread_mutex_lock(&(ssck->mutex_swnd));
		ssck->ssthresh = max(1, ssck->cwnd >> 1);
		ssck->cwnd = 1;
		ssck->swnd = min(min(ssck->cwnd, ssck->rwnd), MAX_SWND);
		ssck->is_timeout = true;
		ssck->currptr = ssck->baseptr + 1;
		printf("swnd = %d (by timeout_t)\n", ssck->swnd);
		pthread_mutex_unlock(&(ssck->mutex_swnd));
		pthread_kill(ssck->rate_t, SIGUSR1);
	}
	pthread_exit(0);
}

void* rate_control(void *sck_ptr)
{
	struct ssock *ssck = (struct ssock *) sck_ptr;
    int byte_seq_num = 0;
    struct sockaddr_in addr = sid_to_addr[ssck->sid];
 	signal(SIGUSR1, sig_handler);
	while (1) {
        while (1) {
    		pthread_mutex_lock(&(ssck->mutex_swnd));
    		if ((ssck->currptr - ssck->baseptr) > (ssck->swnd)) {
    			pthread_mutex_unlock(&(ssck->mutex_swnd)); 	
    			break;
    		}
	        int offset = ssck->currptr - ssck->baseptr;
        	if (offset > (ssck->pkt_buffer).size()) {
        		assert(offset == ((ssck->pkt_buffer).size() + 1));
        		int dlen;
        		pthread_mutex_unlock(&(ssck->mutex_swnd));
	            char* data = get_data(ssck, &dlen);
	            pthread_mutex_lock(&(ssck->mutex_swnd));
	            (ssck->pkt_buffer).push_back(create_packet(byte_seq_num, data, dlen));
	            byte_seq_num += ((ssck->pkt_buffer).back()).content.dpkt.len;
	            offset = (ssck->pkt_buffer).size();
        	}
        	assert(offset >= 1);
            udp_send(addr, (ssck->pkt_buffer)[offset - 1]);
            ssck->currptr ++;
			printf("swnd = %d (by rate_t)\n", ssck->swnd);
    		pthread_mutex_unlock(&(ssck->mutex_swnd)); 
        }

	   	if (ssck->skip_timeout) {
        	ssck->skip_timeout = false;
        } else {
			pthread_create(&(ssck->timeout_t), NULL, timeout_handler, (void *)(ssck));
	        pause();
	        ssck->timeout_t = -1;
        }
    }
}

static void update_window(struct ack_packet apkt, struct ssock *ssck)
{ 
	pthread_mutex_lock(&(ssck->mutex_swnd));
	if (ssck->curr_ack > apkt.ack) {
		// ignore previous dupacks
		pthread_mutex_unlock(&(ssck->mutex_swnd));
		return ;		
	}
	if (ssck->timeout_t != -1)
		pthread_kill(ssck->timeout_t, SIGUSR1); /* wake up timeout thread */
	ssck->is_timeout = false;
	if (ssck->curr_ack == apkt.ack) {
		ssck->num_dupacks++;
		if (ssck->num_dupacks == 3) {
			// triple dupacks
			int tmp = ssck->ssthresh;
			ssck->ssthresh = max((int)1, ssck->cwnd >> 1);
			ssck->cwnd = tmp;
			ssck->currptr = ssck->baseptr + 1;
			ssck->skip_timeout = true;
			ssck->swnd = min(min(ssck->cwnd, ssck->rwnd), MAX_SWND);
		}
	} else {
		ssck->num_dupacks = 0;
		// new ack handler
		ssck->rwnd = apkt.rwnd;
		if (ssck->cwnd < ssck->ssthresh) {
			// slow start
			ssck->cwnd ++;
		} else {
			// Congestion Avoidance using AIMD
			/* Do additive increase */
			ssck->cwnd_cnt += apkt.ack - ssck->curr_ack;
			if (ssck->cwnd_cnt >= ssck->cwnd) {
				int tmp = ssck->cwnd_cnt / ssck->cwnd;
				ssck->cwnd_cnt %= ssck->cwnd;
				ssck->cwnd += tmp;
			}
		}
		ssck->swnd = min(min(ssck->cwnd, ssck->rwnd), MAX_SWND);

		int tmp = (apkt.ack) / MAX_DATA_SIZE - ssck->baseptr - 1;
    	ssck->baseptr = (apkt.ack) / MAX_DATA_SIZE - 1;
    	ssck->currptr = max(ssck->currptr, ssck->baseptr + 1);
    	while (tmp --) {
    		assert(ssck->pkt_buffer.empty()== false);
    		(ssck->pkt_buffer).pop_front();
    	}
	}
	ssck->prev_ack = ssck->curr_ack;
	ssck->curr_ack = apkt.ack;
	printf("swnd = %d (by update_window())\n", ssck->swnd);
	pthread_mutex_unlock(&(ssck->mutex_swnd));
	pthread_kill(ssck->rate_t, SIGUSR1);
}

static inline void send_ack(struct sockaddr_in addr, int ack, int rwnd, bool is_synack = false)
{
	struct packet pkt;
	pkt = create_packet(ack, rwnd, is_synack);	
	udp_send(addr, pkt);
}

static int get_sessid(struct sockaddr_in addr)
{
	int n = sid_to_addr.size();
	for (int i = 0; i < num_conn; i++) {
		if ((sid_to_addr[i].sin_addr.s_addr == addr.sin_addr.s_addr) and 
			(sid_to_addr[i].sin_port == addr.sin_port)) 
			return i;
	}
	error("Socket ID not found");
}

static inline struct rsock* get_rsock(struct sockaddr_in addr)
{
	int n = get_sessid(addr);
	assert(sid_to_rsock.find(n) != sid_to_rsock.end());
	return sid_to_rsock[n];
}

static inline struct ssock* get_ssock(struct sockaddr_in addr)
{
	int n = get_sessid(addr);
	assert(sid_to_ssock.find(n) != sid_to_ssock.end());
	return sid_to_ssock[n];
}

static ssize_t recv_buffer_handle(struct data_packet dpkt, struct sockaddr_in addr)
{
	struct rsock *rsck = get_rsock(addr);
	if (dpkt.seq < rsck->prev_ack) {
		// ignore dup(already written in buffer)
		return 0;
	}
	
	pthread_mutex_lock(&(rsck->mutex_rb));
	while ((dpkt.seq + dpkt.len - rsck->last_read_seq) > MAX_RBUFFER_SIZE) {
		pthread_mutex_unlock(&(rsck->mutex_rb));
		pause();
		pthread_mutex_lock(&(rsck->mutex_rb));
	}
	
	if ((dpkt.seq + dpkt.len - 1 - rsck->last_read_seq) > (rsck->recv_buffer).size())
		rsck->recv_buffer.resize((dpkt.seq + dpkt.len - 1 - rsck->last_read_seq), {'#', false});

	int start = dpkt.seq - rsck->last_read_seq - 1;
	for (int i = start; i < start + dpkt.len; i++) {
		if (rsck->recv_buffer[i].second == false) 
			rsck->rwnd--;
		rsck->recv_buffer[i] = {dpkt.buf[i- start], true};
	}

	int new_ack = rsck->recv_buffer.size() + rsck->last_read_seq + 1;
	for (int i = (rsck->prev_ack - rsck->last_read_seq - 1); i < rsck->recv_buffer.size(); i++)
		if (rsck->recv_buffer[i].second == false) {
			new_ack = i + rsck->last_read_seq + 1;
			break;
		}
	send_ack(addr, new_ack, rsck->rwnd);
	rsck->prev_ack = new_ack;
	pthread_mutex_unlock(&(rsck->mutex_rb));
	pthread_kill(main_t, SIGUSR1);
	return 0;
}

static inline void conn_request_handler(struct syn_packet spkt, struct sockaddr_in addr)
{
	pthread_mutex_lock(&mutex_backlogQ);
	if (backlogQ.size() != max_backlogs) {
		send_ack(addr, spkt.isn + 1, MAX_RBUFFER_SIZE, true);
		int sid = add_conn(addr);
		backlogQ.push(sid);
		pthread_kill(main_t, SIGUSR1);
	}
	pthread_mutex_unlock(&mutex_backlogQ);
}

static inline void syn_ack_handle(struct ack_packet apkt)
{
	pthread_kill(main_t, SIGUSR1);
}

static inline void parse_packet(struct packet pkt, struct sockaddr_in addr)
{
	if (pkt.type == ACKP) {
		update_window(pkt.content.apkt, get_ssock(addr));
	} else if (pkt.type == DATAP) {
		cout << "received DATAP\n";
		recv_buffer_handle(pkt.content.dpkt, addr);
	} else if (pkt.type == SYNP) {
		cout << "received SYNP\n";
		conn_request_handler(pkt.content.spkt, addr);
	} else if (pkt.type == SYN_ACKP) {
		cout << "received SYN_ACKP\n";
		syn_ack_handle(pkt.content.apkt);
	}
}

void* receiver(void *)
{
	signal(SIGUSR1, sig_handler);
	while (1) {
		struct sockaddr_in addr;
		struct packet pkt = udp_receive(&addr);
		parse_packet(pkt, addr);
	}
}

ssize_t app_accept(char* src_addr, int *port)
{
	signal(SIGUSR1, sig_handler);
	pthread_mutex_lock(&mutex_backlogQ);
	while (backlogQ.empty()) {
		pthread_mutex_unlock(&mutex_backlogQ);
		pause();
		pthread_mutex_lock(&mutex_backlogQ);
	}
	int sid = backlogQ.front(); 
	backlogQ.pop();
	struct sockaddr_in addr = sid_to_addr[sid];
	src_addr = inet_ntoa(addr.sin_addr);
	*port = ntohs(addr.sin_port);

	pthread_mutex_unlock(&mutex_backlogQ);
	return sid;
}

ssize_t app_recv(int sid, void* buf, int len)
{
	signal(SIGUSR1, sig_handler);
	
	assert(sid_to_rsock.find(sid) != sid_to_rsock.end());
	struct rsock *rsck = sid_to_rsock[sid];
	pthread_mutex_lock(&(rsck->mutex_rb));
	
	while (len > (rsck->prev_ack - rsck->last_read_seq - 1)) {
		pthread_mutex_unlock(&(rsck->mutex_rb));
		pause();
		pthread_mutex_lock(&(rsck->mutex_rb));
	}

	for (int i = 0; i < len; i++) {
		assert(rsck->recv_buffer.front().second);
		*((char *)buf + i) = rsck->recv_buffer.front().first;
		rsck->recv_buffer.pop_front();
		rsck->rwnd ++;
		rsck->last_read_seq ++;
	}

	pthread_mutex_unlock(&(rsck->mutex_rb));
	pthread_kill(recv_t, SIGUSR1);
	return 0;
}
