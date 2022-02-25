#ifndef _HELPERS_H
#define _HELPERS_H 1

#define TOPICLEN 50
#define IDLEN 11
#define DATALEN 1500
#define UDPBUFLEN 1551
#define IPADDRLEN 16

#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(EXIT_FAILURE);				\
		}							\
	} while(0)


// structura de mesaje pe care serverul le receptioneaza de la clientul TCP
struct __attribute__((__packed__)) tcpToServerMsg {
	char ID[IDLEN];
	char topic[TOPICLEN];
	char sf;
};

// structura de mesaje pe care clientul TCP le receptioneaza de la server
struct __attribute__((__packed__)) serverToTcpMsg {
	char topic[TOPICLEN];
	char dataType;
	char data[DATALEN];
	char ip[IPADDRLEN];
	int port;
};

#endif
