#include <bits/stdc++.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <chrono>
#include <time.h>
#define INIT_MONEY 100

using namespace std;

typedef struct _user{
    int nid;
    string ip_address;
    int port_no;
    int money;
    _user(int a, string b, int c, int d = INIT_MONEY) {nid = a, ip_address =b, port_no = c; money = d;}
} User;

int parent_socket;

struct trans {
	string time_stamp;
	int src, dest, money;
};

struct vote {
	string time_stamp;
	int voter;
	char V;
	int src, dest, money;
};

struct packet {
	int ty;
	union {
		struct trans ts;
		struct vote vt;
	} content;
};

void error(string msg) {
	perror(msg.c_str());
	exit(1);
}

typedef struct Trans{
	int src, dest;
	string time_stamp;
	const bool operator < (const Trans &t) const {
		return time_stamp < t.time_stamp;
	}
}Trans;

typedef struct Vote{
	int src, dest, voter;
	string time_stamp;
	const bool operator < (const Vote &t) const {
		return time_stamp < t.time_stamp;
	}
}Vote;

pthread_t recv_t;

vector<User> user_info;
vector<vector<int>> adj;
map<Trans, bool > hist;
map<Trans, pair<int, int> > all_votes;
map<Vote, bool> hist_vote;
map<pair<string, int>, int> ipptoid;
vector<struct trans> ledger;
int my_id;

void viewledger()
{
	for (auto &ld : ledger) {
		cout << ld.time_stamp << " " << ld.src << " " << ld.dest << " " << ld.money << endl;
	}
}

void sys_init()
{
	ifstream fp("topo.cnf");
	int nid;
	string saddr;
	int sport;
	string str;
	int i = 0;
	while (getline(fp, str)) {
		istringstream ss(str);
		ss >> nid >> saddr >> sport;
		user_info.push_back(User(nid, saddr, sport));
		ipptoid.insert({{user_info[i].ip_address, user_info[i].port_no}, i++});
		vector<int> tmp;
		int a;
		while (ss >> a) tmp.push_back(a);
		adj.push_back(tmp);
	}
}

inline int get_id(struct sockaddr_in addr)
{
	string ip_address = inet_ntoa(addr.sin_addr);
	int port = ntohs(addr.sin_port);
	if (ipptoid.find({ip_address, port}) != ipptoid.end()) return ipptoid[{ip_address, port}];
	error("entry not found in friend list");
}

void broadcast_transaction(struct trans tpkt, int from)
{
	Trans tt;
	tt.src = tpkt.src;
	tt.dest = tpkt.dest;
	tt.time_stamp = tpkt.time_stamp;
	all_votes[tt] = {0, 0};

	if (hist.find(tt) != hist.end()) {
		// dup transaction
		// ignore
	} else {
		hist[tt] = true;
	    // broadcast
		struct packet pkt;
		pkt.ty = 0;
		pkt.content.ts = tpkt;
	    for (auto &nn : adj[my_id]) {
	    	if (nn == from) continue;
	    	struct sockaddr_in address;
			address.sin_family = AF_INET;
			inet_aton(user_info[nn].ip_address.c_str(),
					(in_addr*)&address.sin_addr.s_addr);
			address.sin_port = htons(user_info[nn].port_no);
		    if (sendto(parent_socket, &pkt, sizeof(pkt), 0, (struct sockaddr*)&address, sizeof(address)) < 0)
	        	error("ERROR writing to socket");
	    }
	}
}

void broadcast_vote(struct vote pkt, int from)
{
	Vote tt;
	tt.src = pkt.src;
	tt.dest = pkt.dest;
	tt.voter = my_id;
	tt.time_stamp = pkt.time_stamp;
	if (hist_vote.find(tt) != hist_vote.end()) {
		// dup transaction
		// ignore
	} else {
		hist_vote[tt] = true;
	    // broadcast
	    for (auto &nn : adj[my_id]) {
	    	if (nn == from) continue;
	    	struct sockaddr_in address;
			address.sin_family = AF_INET;
				inet_aton(user_info[nn].ip_address.c_str(),
			(in_addr*)&address.sin_addr.s_addr);
			address.sin_port = htons(user_info[nn].port_no);
			
		    if (sendto(parent_socket, &pkt, sizeof(pkt), 0, (struct sockaddr*)&address, sizeof(address)) < 0)
	        	error("ERROR writing to socket");
	    }
	}
}

int validate_transaction(struct trans pkt, int from)
{
	int nm = pkt.money;
	if (nm > user_info[pkt.src].money) {
		return -1;
	} else {
		return 0;
	}
}

struct vote vote_transaction(struct trans pkt, int from, int valid)
{
	struct vote vt;
	vt.time_stamp = pkt.time_stamp;
	vt.voter = my_id;
	vt.src = pkt.src;
	vt.dest = pkt.dest;
	vt.money = pkt.money;
	vt.V = ((valid == -1) ? 'N' : 'Y');
	return vt;
}

void reach_consensus(struct trans pkt)
{
	Trans tt;
	tt.src = pkt.src;
	tt.dest = pkt.dest;
	tt.time_stamp = pkt.time_stamp;

	if (all_votes[tt].first > all_votes[tt].second) {
		// > 50%
		// update ledger
		ledger.push_back(pkt);
		user_info[pkt.src].money -= pkt.money;
		user_info[pkt.dest].money += pkt.money;
	} else {
		// ignore transaction.
	}
	all_votes.erase(tt);
}

void* consensus_handler(void *pkt)
{
	struct trans *tpkt = (struct trans*) pkt;
	sleep(100);
	reach_consensus(*tpkt);
	pthread_exit(0);
}

void receive_transaction(struct trans pkt, int from)
{
	broadcast_transaction(pkt, from); 
	int valid = validate_transaction(pkt, from);
	struct vote vpkt = vote_transaction(pkt, from, valid);
	broadcast_vote(vpkt, -1);
	pthread_t consensus_t;
	struct trans *tpkt = new struct trans;
	*tpkt = pkt;
	pthread_create(&consensus_t, NULL, consensus_handler, (void *)(tpkt));
}

void receive_vote(struct vote vpkt, int from)
{
	Trans tt;
	tt.src = vpkt.src;
	tt.dest = vpkt.dest;
	tt.time_stamp = vpkt.time_stamp;

	if (all_votes.find(tt) == all_votes.end()) {
		// already cancled trans
		return;
	}
	if (vpkt.V == 'Y') all_votes[tt].first++;
	else all_votes[tt].second++;
	broadcast_vote(vpkt, from);
}

void parse_packet(struct packet pkt, int from)
{
	if (pkt.ty == 0) {
		receive_transaction(pkt.content.ts, from);
	} else {
		receive_vote(pkt.content.vt, from);
	}
}

void* recv_pkt(void *)
{
	while (1) {
		struct packet pkt;
		struct sockaddr_in saddr;
		int addr_len;
		recvfrom(parent_socket, &pkt, sizeof(pkt), 0, (struct sockaddr*) &saddr, (socklen_t *)&addr_len);
		parse_packet(pkt, get_id(saddr));
	}
	pthread_exit(0);
}

void initiate_transaction(string sss)
{
	istringstream ss(sss);
	string tt;
	int to, mny;
	ss >> tt >> to >> mny;
	if (tt != "TRAN")  return viewledger();
	struct trans tpkt;
	tpkt.src  = my_id;
	tpkt.dest = to;
	tpkt.money = mny;
	time_t curtime;
	time(&curtime);
	tpkt.time_stamp = ctime(&curtime);
	receive_transaction(tpkt, my_id);
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	// create a server socket
	parent_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (parent_socket < 0) error("ERROR opening socket");

	struct sockaddr_in address;
	int addrlen, max_sd;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((unsigned short)atoi(argv[1]));
	addrlen = sizeof(address);

	if (bind(parent_socket, (struct sockaddr*)&address, sizeof(address)) < 0)
		error("bind failed");

	sys_init();	
	my_id = get_id(address);
	pthread_create(&recv_t, NULL, recv_pkt, NULL);

	while(1) {
		string ss;
		getline(cin, ss);
		initiate_transaction(ss);
	}
}
