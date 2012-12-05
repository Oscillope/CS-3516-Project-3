//Libraries
#include "cs3516sock.h"
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <iostream>
#include <map>
using namespace std;

#define TRUE 1
#define FALSE 0
#define MAX_PACKET_SIZE 1024
#define MAX_SIZE 4096

void router(void);
void host(void);
void readConfig(string filename);
int nextSize(char getSize[MAX_SIZE], int cursor);

struct globalConf {
	int queueLength;
	int defaultTTL;
};
struct routerConf {
	int sendDelay;
	int router2ID;
	int router2Delay;
};
struct hostConf {
	int sendDelay;
	string overlayPrefix;
	int endHostID;
	int endHostDelay;
};
globalConf configuration;
map<int,string> routerIPs;
map<int,string> endIPs;
map<int,routerConf> routerRouter;
map<int,hostConf> routerEnd;

int main(int argc, char** argv) {
    int o;
    string configPath;
    bool isRouter = FALSE;
    while((o = getopt(argc, argv, "rh")) != -1) {
		switch(o) {
			case 'r':
				//enter router mode
				isRouter = TRUE;
				break;
			case 'h':
				//enter host mode
				isRouter = FALSE;
				break;
			case '?':
			    fprintf(stderr, "Unknown option -%c.\n", optopt);
			    printf("Specify host with -h or router with -r\n");
			    exit(0);
				break;
		}
	}
	configPath = (string)argv[optind];
	if(configPath.empty()) {
		printf("Please, you must provide a filename.\n");
		printf("Usage: overlay [-r -h] config.conf\n");
		exit(1);
	}
	readConfig(configPath);
	if(isRouter) router();
	else host();
	return 0;
}

void readConfig(string filename) {
	FILE *fp;
	char* cname;
	char config[MAX_SIZE];
	char* curItem;
	curItem = new char[MAX_SIZE];
	int size;
	cname = new char[filename.size()+1];
	strcpy(cname, filename.c_str());
	fp = fopen(cname, "r");
	fread(config, sizeof(char), MAX_SIZE, fp);
	#ifdef DEBUG
		if(!ferror(fp)) printf("Successfully read the config file! Let me try to parse that for you.\n");
		else fprintf(stderr, "An error occurred while reading the config file. Sorry about that.\n");
	#endif
	unsigned int i = 0;
	while(i != strlen(config)+1)
		switch(config[i]) {
			case '0':
				#ifdef DEBUG
					printf("Global configuration found! I'm so excited!\n");
				#endif
				i += 2;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				i += size+1;
				configuration.defaultTTL = atoi(curItem);
				curItem = new char[MAX_SIZE];
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				configuration.queueLength = atoi(curItem);
				#ifdef DEBUG
					printf("Parsed global configuration as TTL = %d and Queue Length = %d. Good choices!\n", configuration.defaultTTL, configuration.queueLength);
				#endif
				break;
		};
}

int nextSize(char getSize[MAX_SIZE], int cursor) {
	int size = 0;
	while(getSize[cursor] != ' ' && getSize[cursor] != '\n') {
		size++;
		cursor++;
	}
	#ifdef DEBUG
		printf("Size of next element: %d\n", size);
	#endif
	return size;
}

void router(void){
    printf("I am a router!\n");
    //bind socket 
    int sockfd = create_cs3516_socket();
    while(TRUE){
        char receivebuffer[MAX_PACKET_SIZE];
        cs3516_recv(sockfd, receivebuffer, MAX_PACKET_SIZE);
        iphdr *ip = (iphdr*)receivebuffer;
        (ip->ttl)--;
        if((ip->ttl)>0){
            //TODO fix destination host
            cs3516_send(sockfd, receivebuffer, MAX_PACKET_SIZE, ip->saddr);
        } else {
            //drop the packet
        }
    }
}
void host(void){
    printf("I am a host!\n");
    //bind socket 
    int sockfd = create_cs3516_socket();
    int datalen = 10;
    char data[10];
    struct iphdr overlayIP;
    //We're using IPv4
    overlayIP.version=4;
    //TODO figure out what this should actually be...
    overlayIP.tos=0;
    //TODO set TTL based on input read
    overlayIP.ttl=5;
    //We're going to be using UDP for all our packets
    overlayIP.protocol=IPPROTO_UDP;
    //TODO calculate packet sizes and checksums
    //TODO read addresses from file
    char* srcip = (char*)"1.1.1.1";
    char* dstip = (char*)"2.2.2.2";
    //set ip addresses in header
    inet_pton(AF_INET, srcip, &overlayIP.saddr);
    inet_pton(AF_INET, dstip, &overlayIP.daddr);
    
    struct udphdr overlayUDP;
    //We don't know what ports to use so...
    overlayUDP.source=1337;
    overlayUDP.dest=1337;
    //We won't calculate the checksum for now
    overlayUDP.check=0;
    //just the header and the data
    overlayUDP.len = sizeof(struct udphdr)+datalen;
    //no options in our IP header, just the header and the UDP packet
    overlayIP.tot_len = sizeof(iphdr)+overlayUDP.len;
    
    char *packetbuffer = (char*)malloc(overlayIP.tot_len);
    //put ip header at start of packet
    memcpy(packetbuffer, &overlayIP, sizeof(struct iphdr));
    //put udp header after ip header
    memcpy(packetbuffer+sizeof(struct iphdr),&overlayUDP,sizeof(struct udphdr));
    //put data after udp header
    memcpy(packetbuffer+sizeof(struct iphdr)+sizeof(struct udphdr),data,datalen);
    //TODO determine router IP and fix destination IP
    cs3516_send(sockfd, packetbuffer, overlayIP.tot_len, overlayIP.saddr);
    //once we have sent the packet we don't need to keep it
    free(packetbuffer);
    //TODO non blocking receives and processing of received packets
    char receivebuffer[MAX_PACKET_SIZE];
    cs3516_recv(sockfd, receivebuffer, MAX_PACKET_SIZE);
}
