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
	int socket;
    socket = create_cs3516_socket();
    char* recvbuf;
    cs3516_recv(socket, recvbuf, 1024);
    printf("%s\n",recvbuf);
	return 0;
}
