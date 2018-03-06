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

#define MAXF 30
#define CONN_CLOSE_TIMER 60
#define ACTIVITY_TIMER 5

using namespace std;

typedef struct {
    string name;
    string ip_address;
    int port_no;
} User;

typedef struct client {
    string ip_address;
    string name;
    chrono::high_resolution_clock::time_point lastActiveTime;
} Client;

using mp_sidtoC = map<int, Client>;
using mp_ntoS = map<string, pair<int, chrono::high_resolution_clock::time_point>>;
