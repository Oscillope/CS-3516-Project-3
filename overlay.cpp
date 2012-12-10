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
#include <sys/select.h>
#include <fcntl.h>
#include <assert.h>
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
#define MAX_PACKET_SIZE 1000
#define ROUTER_QUEUE_SIZE 10
#define MAX_SIZE 4096
#define MAX_LINE_SIZE 128

void router(void);
void host(void);
void makeTrie(void);
void readConfig(string filename);
int nextSize(char getSize[MAX_SIZE], int cursor);
int writetofile(char* buffer, size_t size);
int recvFile(int sock, char *buffer, int buff_size, struct sockaddr *from);
void logRecv();
void writetolog(string srcip, string dstip, int ipident, string statuscode, string nexthop);

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
				cout << "Temp: " << temp << endl;
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
        #ifdef DEBUG
			cout << "Testing trie." << endl;
			int testaddr;
			inet_pton(AF_INET, "1.2.3.1", (void *)&testaddr);
			cout << testaddr << hosts.search(testaddr) << endl;
		#endif
    }
    
}

void router(void) {
	#ifdef DEBUG
		cout << "I am router #" << hostID << "!" << endl;
	#endif
	routerConf myconf = routerRouter[hostID];
    //bind socket 
    int sockfd = create_cs3516_socket();
    //bool sent = FALSE;
    //initialize for select() call
    fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	//We want a truly nonblocking call so...
	struct timeval timeoutval;
	timeoutval.tv_sec = 10;
	timeoutval.tv_usec = 0;
    map<string, deque<struct message *> > outputbuffers;
    map<int, hostIP>::iterator it;
    struct cidrprefix tempFix;
    for(it = endIPs.begin(); it != endIPs.end(); it++) {
		inet_pton(AF_INET, (*it).second.overlay.c_str(), (void *)&tempFix.prefix);
		tempFix.size = 32;
		hosts.insert(tempFix, (*it).second.real);
		outputbuffers[(*it).second.real] = *(new deque<struct message *>);	//Try to access the output buffer for this IP, but since it won't exist, create one.
		#ifdef DEBUG
			//cout << outputbuffers.find((*it).second.real) << endl;
			cout << "I just inserted " << hosts.search(tempFix.prefix) << endl;
		#endif
	}
		
    while(TRUE){
        //check if we have anything to read
        FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
        select(sockfd+1, &readfds, NULL, NULL, &timeoutval);
        //while we have a packet to receive, handle it
        if(FD_ISSET(sockfd, &readfds)){
			#ifdef DEBUG
				cout << "Received a packet!" << endl;
			#endif
            struct message *receivemessage = new struct message();
            receivemessage->buffer = (char*)malloc(MAX_PACKET_SIZE);
            cs3516_recv(sockfd, receivemessage->buffer, MAX_PACKET_SIZE);
            time(&(receivemessage->recvtime));
            iphdr *ip = (iphdr*)(receivemessage->buffer);
	        
	        //get the real ip address of the destination
	        string interface = hosts.search((uint32_t)(ip->daddr)); 
	        cout << "Going to router: " << interface << endl;
	        deque<struct message *> outputqueue;
	        //Get some info from the packet for logging purposes
	        char srcstr[INET_ADDRSTRLEN], dststr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip->saddr), srcstr, INET_ADDRSTRLEN);
	        inet_ntop(AF_INET, &(ip->daddr), dststr, INET_ADDRSTRLEN);
	        cout << "This fella is going to " << dststr << endl;
	        short id = ntohs(ip->id);
	        //get the output queue
	        cout << "Interface: " << interface << endl;
	        if(outputbuffers.find(interface)!=outputbuffers.end()){
	            outputqueue = outputbuffers.find(interface)->second;
	        } else {
	            //we don't know who this is! Log it and drop that packet like it's hot
	            writetolog(srcstr, dststr, id, "NO_ROUTE_TO_HOST", "");
	            continue;
	        }
	        //decrement the ttl value
            (ip->ttl)--;
            if((ip->ttl)>0){
                if(outputqueue.size()<=configuration.queueLength){
                    outputbuffers[interface].push_back(receivemessage);
                    writetolog(srcstr, dststr, id, "SENT_OK", interface);
                } else {
                    //drop the packet and log
                    writetolog(srcstr, dststr, id, "MAX_SENDQ_EXCEEDED", "");
                }
            } else {
                //drop the packet and log
                writetolog(srcstr, dststr, id, "TTL_EXPIRED", "");
            }
        }
        //look at queues to see if any send delays have elapsed
        //TODO per-queue send delay as specified by the assignment
        //if(!sent)
	    for(map<string, deque<struct message *> >::iterator i = outputbuffers.begin(); i != outputbuffers.end(); i++) {
	    	string interface = (*i).first;
	    	deque<struct message *> buffer = (*i).second;
	    	//cout << "Interface: " << interface << endl;
	    	//cout << "Size: " << buffer.size() << endl;
		    if(buffer.size()>0){
		        struct message* currentmsg = buffer.front();
		        time_t currenttime;
		        time(&currenttime);
		        double waited = difftime(currenttime, currentmsg->recvtime);
		        if(waited>((double)myconf.sendDelay/1000)){
		            unsigned int interfacebytes;
		            //convert address to bytes
		            inet_pton(AF_INET, (char*)interface.c_str(), (void *)&interfacebytes);
		            cout << "Down to the nitty-gritty: sending the packet." << endl;
		            int status = cs3516_send(sockfd, currentmsg->buffer, MAX_PACKET_SIZE, interfacebytes);
                    //if(status) sent = TRUE;
                    if(!status) fprintf(stderr, "There was an error sending the file. No bytes were sent.");
                    //free(currentmsg->buffer);
                    delete currentmsg;
                    outputbuffers[interface].pop_front();
                }
		    }
	    }
    }
}
void host(void){
    FILE *fp = fopen("send_config.txt", "r");
    char str[MAX_LINE_SIZE];
    char dstip[INET_ADDRSTRLEN];
    char srcport[6], destport[6];
    if(fgets(str, MAX_LINE_SIZE, fp)!=NULL){
        char * pch;
        pch = strtok (str," ");
        strcpy(srcport, pch);
        pch = strtok (NULL, " ");
        strcpy(destport, pch);
        pch = strtok (NULL, " ");
    }
    strcpy(dstip, str);
    fclose(fp);
    unsigned long routerIP;
    inet_pton(AF_INET, "10.10.10.89", (void *)&routerIP);
    
    fp = fopen("send_body.txt", "rb");
    fseek(fp, 0L, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    char *fbuffer = new char[fsize];
    fread(fbuffer, fsize, 1, fp);
    fclose(fp);
    cout << "Sending file of size: " << fsize << " to " << dstip << endl;
	#ifdef DEBUG
		cout << "I am end host #" << hostID << "!" << endl;
	#endif
    //bind socket 
    int sockfd = create_cs3516_socket();
    struct iphdr overlayIP;
    //We're using IPv4
    overlayIP.version=4;
    //TODO figure out what this should actually be...
    overlayIP.tos=0;
    overlayIP.ttl=configuration.defaultTTL;
    //We're going to be using UDP for all our packets
    overlayIP.protocol=IPPROTO_UDP;
    //TODO calculate checksums
    char* srcip = (char*)endIPs[hostID].overlay.c_str();
    //set ip addresses in header
    inet_pton(AF_INET, srcip, &overlayIP.saddr);
    inet_pton(AF_INET, dstip, &overlayIP.daddr);
    
    struct udphdr overlayUDP;
    //We don't know what ports to use so...
    overlayUDP.source=htons(atoi(srcport));
    overlayUDP.dest=htons(atoi(destport));;
    //We won't calculate the checksum for now
    overlayUDP.check=0;
    for(int i=0; i<fsize; i+=MAX_PACKET_SIZE){
        overlayIP.id=i/MAX_PACKET_SIZE;
        int msgsize;
        if((fsize-i)<MAX_PACKET_SIZE){
            //final segment (size less than 1000)
            msgsize = fsize-i;
        } else {
            //segment of size 1000
            msgsize = MAX_PACKET_SIZE;
        }
        overlayUDP.len = sizeof(struct udphdr)+msgsize;
        //just the header and the data
        overlayIP.tot_len = sizeof(iphdr)+overlayUDP.len;
        //no options in our IP header, just the header and the UDP packet
        char *packetbuffer = (char*)malloc(overlayIP.tot_len);
        //put ip header at start of packet
        memcpy(packetbuffer, &overlayIP, sizeof(struct iphdr));
        //put udp header after ip header
        memcpy(packetbuffer+sizeof(struct iphdr),&overlayUDP,sizeof(struct udphdr));
        //put data after udp header
        memcpy(packetbuffer+sizeof(struct iphdr)+sizeof(struct udphdr),fbuffer+i,msgsize);
        //TODO determine router IP and fix destination IP
        cout << overlayIP.tot_len << endl;
        cout << packetbuffer << endl;
        
        cs3516_send(sockfd, packetbuffer, overlayIP.tot_len, routerIP);
        //once we have sent the packet we don't need to keep it
        free(packetbuffer);
        cout << "Sent " << msgsize << " bytes." << endl;
    }
    delete(fbuffer);
    //TODO non blocking receives and processing of received packets
    //TODO host logging
    char receivebuffer[MAX_PACKET_SIZE];
    struct sockaddr from;
    recvFile(sockfd, receivebuffer, MAX_PACKET_SIZE, &from);
    //logRecv();
    writetofile(receivebuffer, sizeof(receivebuffer));
}

int writetofile(char* buffer, size_t size){
    FILE *fp;
    cout << "Writing a file!" << endl;
    char name[9] = "received";
    fp=fopen(name, "ab");
    size_t written = fwrite(buffer, sizeof(char), size, fp);
    fclose(fp); //we're done writing to the file
    return written!=size;
}

int recvFile(int sock, char *buffer, int buff_size, struct sockaddr *from) {
    socklen_t fromlen;
    int n;
    fromlen = sizeof(struct sockaddr_in);
    n = recvfrom(sock, buffer, buff_size, 0, from, &fromlen);
    cout << "I've got something!" << endl;
    return n;
}

void writetolog(string srcip, string dstip, int ipident, string statuscode, string nexthop){
    char ftime[256];
    time_t ctime = time(NULL);
    struct tm *thetime;
    thetime = localtime(&ctime);
    strftime(ftime, 256, "%s", thetime);
    string timestr = string(ftime);
    //UNIXTIME SOURCE_OVERLAY_IP DEST_OVELRAY_IP IP_IDENT STATUS_CODE [NEXT_HOP]

    FILE *fp;
    fp=fopen("ROUTER_control.txt", "a");
    fprintf(fp, "%s %s %s %d %s %s\n", (char*)timestr.c_str(), (char*)srcip.c_str(), (char*)dstip.c_str(), ipident, (char*)statuscode.c_str(), (char*)nexthop.c_str());
    fclose(fp);
}
