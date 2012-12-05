//Project Specific Libraries
#include "cs3516sock.h"
#include "trie.h"
//C Standard Libraries
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
//Networking Libraries
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
//C++ Libraries
#include <deque>
#include <sstream>
#include <iostream>
#include <map>
using namespace std;

#define TRUE 1
#define FALSE 0
#define MAX_PACKET_SIZE 1024
#define ROUTER_QUEUE_SIZE 10
#define MAX_SIZE 4096

void router(void);
void host(void);
void readConfig(string filename);
int nextSize(char getSize[MAX_SIZE], int cursor);

struct globalConf {
	int queueLength;
	int defaultTTL;
};
struct hostIP {
	string real;
	string overlay;
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
map<int,hostIP> endIPs;
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
			    cout << "Specify host with -h or router with -r" << endl;
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
	int size, ID;
	cname = new char[filename.size()+1];
	strcpy(cname, filename.c_str());
	fp = fopen(cname, "r");
	fread(config, sizeof(char), MAX_SIZE, fp);
	#ifdef DEBUG
		if(!ferror(fp)) printf("Successfully read the config file! Let me try to parse that for you.\n");
		else fprintf(stderr, "An error occurred while reading the config file. Sorry about that.\n");
	#endif
	unsigned int i = 0;
	while(i != strlen(config)+1) {
		switch(config[i]) {
			case '0':
				#ifdef DEBUG
					printf("Global configuration found! I'm so excited!\n");
				#endif
				i += 2;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				configuration.defaultTTL = atoi(curItem);
				i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				configuration.queueLength = atoi(curItem);
				#ifdef DEBUG
					printf("Parsed global configuration as TTL = %d and Queue Length = %d. Good choices!\n", configuration.defaultTTL, configuration.queueLength);
				#endif
				i += size+1;
				break;
			case '1':
				#ifdef DEBUG
					printf("Ah, I see you've got some router identification. Let me get that sorted out.\n");
				#endif
				i += 2;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				ID = atoi(curItem);
				i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				routerIPs[ID] = (string)curItem;
				#ifdef DEBUG
					cout << "Looks like you've got a router with ID number " << ID << " and real IP " << routerIPs[ID] << "." << endl;
				#endif
				i += size+1;
				break;
			case '2':
				#ifdef DEBUG
					printf("Whoa, some clients! I love those guys.\n");
				#endif
				i += 2;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				ID = atoi(curItem);
				i += size+1;
				size = nextSize(config, i);
				char* realIP = new char[MAX_SIZE];
				for(int j = 0; j < size; j++) realIP[j] = config[i+j];
				realIP[size] = '\0';
				i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				endIPs[ID].real = (string)realIP;
				endIPs[ID].overlay = (string)curItem;
				#ifdef DEBUG
					cout << "Looks like you've got an end host with ID number " << ID << ", real IP " << endIPs[ID].real << ", and overlay IP " << endIPs[ID].overlay << "." << endl;
				#endif
				i += size+1;
				break;
		}
	}
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
    cout << "I am a router!" << endl;
    //bind socket 
    int sockfd = create_cs3516_socket();
    deque<char*> outputbuffer;
    while(TRUE){
        char *receivebuffer = (char*)malloc(MAX_PACKET_SIZE);
        cs3516_recv(sockfd, receivebuffer, MAX_PACKET_SIZE);
        iphdr *ip = (iphdr*)receivebuffer;
        (ip->ttl)--;
        if((ip->ttl)>0 || outputbuffer.size()<=ROUTER_QUEUE_SIZE){
            outputbuffer.push_back(receivebuffer);
        } else {
            //drop the packet
        }
        //TODO fix destination host
        cs3516_send(sockfd, outputbuffer.front(), MAX_PACKET_SIZE, ip->saddr);
        free(outputbuffer.front());
        outputbuffer.pop_front();
    }
}
void host(void){
    cout << "I am a host!" << endl;
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
void writetolog(string srcip, string dstip, string ipident, string statuscode, string nexthop){
    char ftime[256];
    time_t ctime = time(NULL);
    struct tm *thetime;
    thetime = localtime(&ctime);
    strftime(ftime, 256, "%s", thetime);
    stringstream sstream;
    string message;
    string timestr = string(ftime);
    //UNIXTIME SOURCE_OVERLAY_IP DEST_OVELRAY_IP IP_IDENT STATUS_CODE [NEXT_HOP]
    if(nexthop.length()!=0){
        sstream << " " << timestr << " " << srcip << " " << dstip << " " << ipident << " " << statuscode << " " << nexthop << endl;
    } else {
        sstream << " " << timestr << " " << srcip << " " << dstip << " " << ipident << " " << statuscode << endl;
    }
    sstream >> message;
    cout << message;

    FILE *fp;
    fp=fopen("ROUTER_control.txt", "a");
    fputs((char*)message.c_str(), fp);
    fclose(fp);
}
