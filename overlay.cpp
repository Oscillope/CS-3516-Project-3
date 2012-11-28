//Libraries
#include "cs3516sock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

int main(int argc, char** argv) {
	struct addrinfo known, *server;
	memset(&known, 0, sizeof known);
	known.ai_family = AF_UNSPEC;
	known.ai_socktype = SOCK_DGRAM;
	known.ai_flags = AI_PASSIVE;
	
	
	return 0;
}
