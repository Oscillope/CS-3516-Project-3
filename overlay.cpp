//Project Specific Libraries
#include "cs3516sock.h"
#include "trie.h"
//C Standard Libraries
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
//Networking Libraries
#include <ifaddrs.h>
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
#define IP_NOT_FOUND -1
#define MAX_PACKET_SIZE 1024
#define ROUTER_QUEUE_SIZE 10
#define MAX_SIZE 4096

void router(void);
void host(void);
void makeTrie(void);
void readConfig(string filename);
int nextSize(char getSize[MAX_SIZE], int cursor);

struct globalConf {
	unsigned int queueLength;
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
struct message {
    time_t recvtime;
    char *buffer;
};
globalConf configuration;
map<int,string> routerIPs;
map<int,hostIP> endIPs;
map<int,routerConf> routerRouter;
map<int,hostConf> routerEnd;
int hostID;
trie hosts;

int main(int argc, char** argv) {
    #ifdef SANDWICH
        printf("I'm a sandwich!\n");
    #endif
    int o;
    string configPath;
    char isRouter = IP_NOT_FOUND;
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
    makeTrie();
    struct ifaddrs *demAddrs;
    if(getifaddrs(&demAddrs)) {
		fprintf(stderr, "Couldn't get real IP address. Aborting.\n");
		exit(1);
	}
	struct in_addr *myAddr;
	char* addrBuf = new char[INET_ADDRSTRLEN];
	struct sockaddr *mysockaddr = (demAddrs->ifa_addr);
	while((strcmp(demAddrs->ifa_name, "eth0") && strcmp(demAddrs->ifa_name, "eth1") && strcmp(demAddrs->ifa_name, "eth2") && strcmp(demAddrs->ifa_name, "eth3") && strcmp(demAddrs->ifa_name, "wlan0"))||(mysockaddr->sa_family!=AF_INET)) {
		#ifdef DEBUG
			cout << "Interface: " << demAddrs->ifa_name << ". NOPE!" << endl;
		#endif
	    demAddrs = demAddrs->ifa_next;
	    if(demAddrs==NULL) {
		    fprintf(stderr, "Couldn't find the specified interface. Aborting. Sorry! Make sure your computer is set up properly.\n");
		    exit(1);
	    }
	    mysockaddr = (demAddrs->ifa_addr);
	}
	#ifdef DEBUG
		cout << "Interface: " << demAddrs->ifa_name << endl;
	#endif
	myAddr = &(((struct sockaddr_in*)mysockaddr)->sin_addr);
	inet_ntop(AF_INET, (void *)myAddr, addrBuf, INET_ADDRSTRLEN);
	#ifdef DEBUG
		printf("My real IP address is: %s\n", addrBuf);
	#endif
	map<int, string>::iterator routeit;;
	for(routeit = routerIPs.begin(); routeit != routerIPs.end(); routeit++) {
		#ifdef DEBUG
			cout << "Testing: " << (*routeit).second << endl;
		#endif
		if(!(*routeit).second.compare((string)addrBuf)) {
			isRouter = TRUE;
			hostID = (*routeit).first;
			break;
		}
	}
	if(isRouter == IP_NOT_FOUND) {
		map<int, hostIP>::iterator endit;
		for(endit = endIPs.begin(); endit != endIPs.end(); endit++) {
			#ifdef DEBUG
				cout << "Testing: " << (*endit).second.real << endl;
			#endif
			if(!(*endit).second.real.compare((string)addrBuf)) {
				isRouter = FALSE;
				hostID = (*endit).first;
				break;
			}
		}
	}
	if(isRouter == IP_NOT_FOUND) {
		fprintf(stderr, "I couldn't find your IP address in my memory banks. Please check to make sure your configuration file is correct.");
		exit(1);
	}
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
    char* realIP = new char[MAX_SIZE];
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
	while(i != strlen(config)) {
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
					cout << "Looks like you've got a router with ID number " << ID << " and IP address " << routerIPs[ID] << "." << endl;
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
            case '3':
                #ifdef DEBUG
                    printf("All right! Defining some router to router links.\n");
                #endif
                i += 2;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
				ID = atoi(curItem);
				i += size+1;
				size = nextSize(config, i);
				int delay1, id2, delay2;
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
                delay1 = atoi(curItem);
				i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
                id2 = atoi(curItem);
                i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
                delay2 = atoi(curItem);
				routerRouter[ID].sendDelay = delay1;
                routerRouter[ID].router2ID = id2;
                routerRouter[ID].router2Delay = delay2;
                #ifdef DEBUG
                    cout << "Ooh! Router " << ID << " has a sending delay of " << routerRouter[ID].sendDelay << ", and router " << routerRouter[ID].router2ID << " has a sending delay of " << routerRouter[ID].router2Delay << "." << endl;
                #endif
				i += size+1;
				break;
            case '4':
                #ifdef DEBUG
                    printf("YAAAAAY END HOST LINKS!\n");
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
                routerEnd[ID].sendDelay = atoi(curItem);
				i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
                routerEnd[ID].overlayPrefix = curItem;
                i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
                routerEnd[ID].endHostID = atoi(curItem);
                i += size+1;
				size = nextSize(config, i);
				for(int j = 0; j < size; j++) curItem[j] = config[i+j];
				curItem[size] = '\0';
                routerEnd[ID].endHostDelay = atoi(curItem);
                #ifdef DEBUG
                    cout << "Whoa there, cowboy! Router " << ID << " has a sending delay of " << routerEnd[ID].sendDelay << ", an overlay prefix of " << routerEnd[ID].overlayPrefix << " and is connected to host " << routerEnd[ID].endHostID << " with a delay of " << routerEnd[ID].endHostDelay << "." << endl;
                #endif
				i += size+1;
				break;
            default:
                fprintf(stderr, "That is not a valid configuration option. Please check your configuration file, as it may have been corrupted.");
                exit(1);
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

void makeTrie(void) {
    map<int, hostConf>::iterator j;
    string::iterator stringit;
    char* temp = new char[16];
    string prestring;
    char* size = new char[2];
    struct cidrprefix tempFix;
    for(j = routerEnd.begin(); j != routerEnd.end(); j++) {
        prestring = (j->second).overlayPrefix;
        for(stringit = prestring.begin(); stringit != prestring.end(); stringit++) {
			if(*stringit == '/') {
				strcpy(temp, prestring.substr(0, distance(prestring.begin(),stringit)).c_str());
				inet_pton(AF_INET, temp, (void *)&tempFix.prefix);
				stringit++;
				size[0] = *stringit;
				stringit++;
				size[1] = *stringit;
				break;
			}
        }
        tempFix.size = (char)atoi(size);
        #ifdef DEBUG
			cout << "Prefix: " << tempFix.prefix << "/" << (int)tempFix.size << " for router: " << routerIPs[j->first] << endl;
		#endif
        hosts.insert(tempFix, routerIPs[j->first]);
    }
}

void router(void) {
	#ifdef DEBUG
		cout << "I am router #" << hostID << "!" << endl;
	#endif
	routerConf myconf = routerRouter[hostID];
    //bind socket 
    int sockfd = create_cs3516_socket();
    //initialize for select() call
    fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	//We want a truly nonblocking call so...
	struct timeval timeoutval;
	timeoutval.tv_sec = 0;
	timeoutval.tv_usec = 0;
    map<string, deque<struct message *> > outputbuffers;
    while(TRUE){
        //check if we have anything to read
        select(sockfd+1, &readfds, NULL, NULL, &timeoutval);
        //while we have a packet to receive, handle it
        //TODO decide if this is good behavior
        while(FD_ISSET(sockfd, &readfds)){
            struct message *receivemessage = new struct message();
            receivemessage->buffer = (char*)malloc(MAX_PACKET_SIZE);
            cs3516_recv(sockfd, receivemessage->buffer, MAX_PACKET_SIZE);
            time(&(receivemessage->recvtime));
            iphdr *ip = (iphdr*)(receivemessage->buffer);
            
            //Get some strings
            /*char srcstr[INET_ADDRSTRLEN], dststr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip->saddr), srcstr, INET_ADDRSTRLEN);
	        inet_ntop(AF_INET, &(ip->daddr), dststr, INET_ADDRSTRLEN);*/
	        
	        //get the real ip address of the destination
	        string interface = hosts.search((uint32_t)(ip->daddr));
	        deque<struct message *> outputqueue;
	        //get the output queue
	        if(outputbuffers.find(interface)!=outputbuffers.end()){
	            outputqueue = outputbuffers.find(interface)->second;
	        } else {
	            //we don't know who this is! Log it and drop that packet like it's hot
	        }
	        //decrement the ttl value
            (ip->ttl)--;
            if((ip->ttl)>0){
                if(outputqueue.size()<=configuration.queueLength){
                    outputqueue.push_back(receivemessage);
                } else {
                    //drop the packet and log
                }
            } else {
                //drop the packet and log
            }
            select(sockfd+1, &readfds, NULL, NULL, &timeoutval);
        }
        //look for the first interface with data to send and send one of their packets
        //TODO decide if there is a better way to do this (one from each buffer?)
	    for(map<string, deque<struct message *> >::iterator i = outputbuffers.begin(); i != outputbuffers.end(); i++) {
	    	string interface = (*i).first;
	    	deque<struct message *> buffer = (*i).second;
		    if(buffer.size()>0){
		        struct message* currentmsg = buffer.front();
		        time_t currenttime;
		        time(&currenttime);
		        int waited = (int)difftime(currentmsg->recvtime, currenttime);
		        //TODO use waited and wait time to determine if a packet should be sent
		        if(waited>((double)myconf.sendDelay/1000)){
		            //4 because IPv4
		            char interfacebytes[4];
		            //convert address to bytes
		            inet_pton(AF_INET, (char*)interface.c_str(), interfacebytes);
		            cs3516_send(sockfd, currentmsg->buffer, MAX_PACKET_SIZE, (unsigned int)(*interfacebytes));
                    free(currentmsg->buffer);
                    delete currentmsg;
                    buffer.pop_front();
                }
		    }
	    }
    }
}
void host(void){
	#ifdef DEBUG
		cout << "I am end host #" << hostID << "!" << endl;
	#endif
    //bind socket 
    int sockfd = create_cs3516_socket();
    int datalen = 10;
    char data[10];
    struct iphdr overlayIP;
    
    
    //We're using IPv4
    overlayIP.version=4;
    //TODO figure out what this should actually be...
    overlayIP.tos=0;
    overlayIP.ttl=configuration.defaultTTL;
    //We're going to be using UDP for all our packets
    overlayIP.protocol=IPPROTO_UDP;
    //TODO calculate packet sizes and checksums
    //TODO read addresses from file
    char* srcip = (char*)endIPs[hostID].overlay.c_str();
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
