#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include "utils.h"

#define RECVMSGLEN 1569

using namespace std;

vector<string> split(string &str, string &delim) {
    vector<string> tokens;
    unsigned long prev = 0, pos = 0;

    do {
        pos = str.find(delim, prev);

        if (pos == string::npos) {
            pos = str.length();
        }

        string token = str.substr(prev, pos - prev);

        if (!token.empty()) {
            tokens.push_back(token);
        }

        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());

    return tokens;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int sockfd, err, flag = 1;
    char buffer[RECVMSGLEN];
    struct sockaddr_in servAddr;
    string comm;
    string delim = " ";

    fd_set readFds;	    // multimea de citire folosita in select()
	fd_set tmpFds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea readFds

    FD_ZERO(&readFds);
	FD_ZERO(&tmpFds);
    FD_SET(STDIN_FILENO, &readFds);

    DIE(argc != 4, "usage: ./subscriber <ID> <IP_SERVER> <PORT_SERVER>\n");

    // daca ID are mai mult de 10 caractere, clientul se opreste
    int idLen = strlen(argv[1]);
    DIE(idLen > 10, "ID must be only 10 characters long\n");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "error at socket\n");

    // dezactivare Neagle
    err = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(err < 0, "error at Neagle disable\n");

    // setare adresa socket
    servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[3]));
	err = inet_aton(argv[2], &servAddr.sin_addr);
	DIE(err < 0, "error at inet_aton\n");

    // connect
    err = connect(sockfd, (struct sockaddr*) &servAddr, sizeof(servAddr));
	DIE(err < 0, "error at connect\n");
    FD_SET(sockfd, &readFds);
    fdmax = sockfd;

    // trimitere ID
    err = send(sockfd, argv[1], IDLEN, 0);
    DIE(err < 0, "couldn't send ID\n");

    while (1) {
        tmpFds = readFds; 
        err = select(fdmax + 1, &tmpFds, NULL, NULL, NULL);
        DIE(err < 0, "error at select in TCP\n");

        if (FD_ISSET(STDIN_FILENO, &tmpFds)) {
            // se citeste de la tastatura
            getline(cin, comm);

            if (comm.compare("exit") == 0) {
                // se inchide clientul
                break;
            }

            vector<string> tokens = split(comm, delim);

            if (tokens.size() > 3 || tokens.size() < 2) {
                printf("Unknown command\n");  
            } else if (tokens[0].compare("subscribe") == 0 && tokens.size() == 3) {
                // comanda de subscribe
                struct tcpToServerMsg *sendMsg = (struct tcpToServerMsg*)calloc(1, sizeof(struct tcpToServerMsg));

                memcpy(sendMsg->ID, argv[1], IDLEN);
                memcpy(sendMsg->topic, tokens[1].c_str(), tokens[1].length());
                sendMsg->sf = atoi(tokens[2].c_str());

                err = send(sockfd, sendMsg, sizeof(struct tcpToServerMsg), 0);
                DIE(err < 0, "send error TCP\n");
                free(sendMsg);

                printf("Subscribed to topic.\n");
            } else if (tokens[0].compare("unsubscribe") == 0 && tokens.size() == 2) {
                // comanda de unsubscribe
                struct tcpToServerMsg *sendMsg = (struct tcpToServerMsg*)calloc(1, sizeof(struct tcpToServerMsg));

                memcpy(sendMsg->ID, argv[1], IDLEN);
                memcpy(sendMsg->topic, tokens[1].c_str(), tokens[1].length());
                sendMsg->sf = 2;

                err = send(sockfd, sendMsg, sizeof(struct tcpToServerMsg), 0);
                DIE(err < 0, "send error TCP\n");
                free(sendMsg);

                printf("Unsubscribed from topic.\n");
            } else {
                // comanda necunonscuta
                printf("Unknown command\n");
            }
        } else if (FD_ISSET(sockfd, &tmpFds)) {
            // primeste un mesaj de la server
            err = recv(sockfd, buffer, sizeof(struct serverToTcpMsg), 0);
            DIE(err < 0, "error at recv in TCP\n");

            if (err == 0) {
                // serverul s-a inchis
                break;
            } else {
                struct serverToTcpMsg *msg = (struct serverToTcpMsg*) buffer;

                if (msg->dataType == 0) {
                    // a primit un int
                    printf("%s:%d - %s - INT - %s\n", msg->ip, msg->port, msg->topic, msg->data);
                } else if (msg->dataType == 1) {
                    // a primit un short
                    printf("%s:%d - %s - SHORT_REAL - %s\n", msg->ip, msg->port, msg->topic, msg->data);
                } else if (msg->dataType == 2) {
                    // a primit un float
                    printf("%s:%d - %s - FLOAT - %s\n", msg->ip, msg->port, msg->topic, msg->data);
                } else if (msg->dataType == 3) {
                    // a primit un string
                    printf("%s:%d - %s - STRING - %s\n", msg->ip, msg->port, msg->topic, msg->data);
                }
            }
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    shutdown(STDIN_FILENO, SHUT_RDWR);
    close(sockfd);
    close(STDIN_FILENO);
}
