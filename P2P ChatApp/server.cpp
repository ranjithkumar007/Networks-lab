#include "server.h"

void error(string msg) {
	perror(msg.c_str());
	exit(1);
}

int main(int argc, char* argv[]) {
	User user_info[MAXF];
	ifstream fp("user-info.txt");
	int i;
	for (i = 0; i < MAXF and fp >> user_info[i].name >> user_info[i].ip_address 
								>> user_info[i].port_no;i++) ;
	int CURF = i;
	int opt = 1;
	int server_socket, addrlen, new_socket, activity, valread, sd;
	mp_sidtoC client_info;  					// map socket id to Client details
	map<pair<string, int>, string> ipptoname;	// map ip address and port number to user name
	map<string, pair<string, int> > nametoipp;  // map user name to ip address and port number
	mp_ntoS nametoidt;  						// map name to id

	int max_sd;
	struct sockaddr_in address;
	char buffer[1025];
	for (int i = 0; i < CURF; i++) {
		nametoipp.insert({user_info[i].name, {user_info[i].ip_address, user_info[i].port_no}});
		ipptoname.insert({{user_info[i].ip_address, user_info[i].port_no}, user_info[i].name});
	}
	fd_set readfds;
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	struct timeval tv;  // time out for select.
	tv.tv_sec = ACTIVITY_TIMER;
	tv.tv_usec = 0;

	// create a server socket
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) error("ERROR opening socket");

	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

	// type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((unsigned short)atoi(argv[1]));

	if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0)
		error("bind failed");

	// try to specify maximum of 5 backlog connections for the server socket
	if (listen(server_socket, 5) < 0) error("listen");

	addrlen = sizeof(address);
	puts("Waiting for connections ...");

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(server_socket, &readfds);
		max_sd = server_socket;
		FD_SET(0, &readfds);

		for (auto cl : client_info) {
			FD_SET(cl.first, &readfds);
			max_sd = max(max_sd, cl.first);
		}
		activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

		if (activity < 0) {
			printf("select error");
			continue;
		} else {
			// if timeout then checks for any inactive client and close if such exists.
			for (auto cl : client_info) {
				if (chrono::duration_cast<std::chrono::seconds>(
							chrono::high_resolution_clock::now() - cl.second.lastActiveTime)
						.count() >= CONN_CLOSE_TIMER) {
					cout << "Closing socket : " << cl.first << " as " << cl.second.name
						<< " is not active recently\n";
					close(cl.first);
					client_info.erase(cl.first);
				}
			}
			for (auto ss : nametoidt) {
				if (chrono::duration_cast<std::chrono::seconds>(
							chrono::high_resolution_clock::now() - ss.second.second)
						.count() >= CONN_CLOSE_TIMER) {
					close(ss.second.first);
					nametoidt.erase(ss.first);
				}
			}
			if (activity == 0) continue;
		}
		// If something happened on the server socket , then its an incoming connection
		if (FD_ISSET(server_socket, &readfds)) {
			if ((new_socket = accept(server_socket, (struct sockaddr*)&address,
							(socklen_t*)&addrlen)) < 0)
				error("accept");
			client_info[new_socket].ip_address = inet_ntoa(address.sin_addr);
			client_info[new_socket].lastActiveTime = chrono::high_resolution_clock::now();
		} else if (FD_ISSET(0, &readfds)) {
			string totalmsg;
			getline(cin, totalmsg);
			string username = totalmsg.substr(0, totalmsg.find('/'));
			string msg = totalmsg.substr(totalmsg.find('/') + 1);
			bool flg = 0;
			char c;
			if (nametoidt.find(username) != nametoidt.end()) {
				if ( send(nametoidt[username].first, msg.c_str(), msg.length(), 0) < 0)
					perror("send");
				continue;
			}
			if (nametoipp.find(username) == nametoipp.end()) {
				cout << username << " not registered!! Message can't be sent\n";
				continue;
			}
			int nsockfd = socket(AF_INET, SOCK_STREAM, 0);
			struct sockaddr_in serveraddr;
			bzero((char*)&serveraddr, sizeof(serveraddr));
			serveraddr.sin_family = AF_INET;
			inet_aton(nametoipp[username].first.c_str(),
					(in_addr*)&serveraddr.sin_addr.s_addr);
			serveraddr.sin_port = htons(nametoipp[username].second);
			if (connect(nsockfd, (sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
				perror("ERROR connecting");
				break;
			}
			nametoidt[username].first = nsockfd;
			nametoidt[username].second = chrono::high_resolution_clock::now();
			msg = string(argv[1]) + "#" + msg;
			if (send(nsockfd, msg.c_str(), msg.length(), 0) < 0) error("send");
		} else {
			for (auto& cl : client_info) {
				if (FD_ISSET(cl.first, &readfds)) {
					// Check if it was for closing , and also read the incoming message
					if ((valread = read(cl.first, buffer, 1024)) == 0) {
						getpeername(cl.first, (struct sockaddr*)&address, (socklen_t*)&addrlen);
						close(cl.first);
						client_info.erase(cl.first);
					} else {
						cl.second.lastActiveTime = chrono::high_resolution_clock::now();
						buffer[valread] = '\0';
						string xxx(buffer);
						if (cl.second.name.length() == 0) {
							string pnum = xxx.substr(0, xxx.find("#"));
							xxx = xxx.substr(xxx.find("#") + 1);
							cl.second.name = ipptoname[{cl.second.ip_address, stoi(pnum)}];
						}
						cout << "Message from " << cl.second.name << " : " << xxx << endl;
					}
					bzero(buffer, 1024);
				}
			}
		}
	}
	return 0;
}
