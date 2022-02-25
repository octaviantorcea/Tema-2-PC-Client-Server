#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include "utils.h"

#define MAXBACKLOGCLN 50
#define TCPBUFLEN 62

using namespace std;

// structura ce contine date specifice unui subscriber
struct subContent {
    int sockfd;                                 // socket descriptorul
    bool online;                                // daca este online sau nu
    map<string, int> subscribedTopics;          // dictionar intre numele topicului si optiunea de SF
    vector<struct serverToTcpMsg*> oldMsg;      // vector de mesaje ce trebuie transmise cand acesta se reconecteaza

    subContent (int sockfd) {
        this->sockfd = sockfd;
        this->online = true;
    }
};

// structura de mesaje pe care serverul le receptioneaza de la clientul UDP
struct __attribute__((__packed__)) udpToServerMsg {
    char topic[TOPICLEN];
    char dataType;
    char data[DATALEN];
};

// functie de parsare a mesajului in functie de dataType
struct serverToTcpMsg* parseMsg(struct udpToServerMsg* udpRM, struct sockaddr_in udpAddr) {
    struct serverToTcpMsg* msg = (struct serverToTcpMsg*)calloc(1, sizeof(struct serverToTcpMsg));

    memcpy(msg->topic, udpRM->topic, TOPICLEN);
    msg->dataType = udpRM->dataType;
    memcpy(msg->ip, inet_ntoa(udpAddr.sin_addr), IPADDRLEN);
    msg->port = ntohs(udpAddr.sin_port);

    if (udpRM->dataType == 0) {
        // INT
        int sign = 1;

        if (udpRM->data[0] == 1) {
            sign = -1;
        }

        uint32_t nr;
        memcpy(&nr, &udpRM->data[1], sizeof(uint32_t));
        nr = ntohl(nr);
        nr *= sign;

        sprintf(msg->data, "%d", nr);
    } else if (udpRM->dataType == 1) {
        // SHORT_REAL
        uint16_t nr;
        memcpy(&nr, udpRM->data, sizeof(uint16_t));
        nr = ntohs(nr);
        float aux = (float)nr / 100;

        sprintf(msg->data, "%.2f", aux);
    } else if (udpRM->dataType == 2) {
        // FLOAT
        int sign = 1;

        if (udpRM->data[0] == 1) {
            sign = -1;
        }

        uint32_t nr;
        memcpy(&nr, &udpRM->data[1], sizeof(uint32_t));
        nr = ntohl(nr);
        float aux = (float) nr;
        aux *= sign;
        
        uint8_t pow;
        memcpy(&pow, udpRM->data + sizeof(uint8_t) + sizeof(uint32_t), sizeof(uint8_t));

        for (int i = 0; i < pow; i++) {
            aux /= 10;
        }

        sprintf(msg->data, "%f", aux);
    } else if (udpRM->dataType == 3) {
        // STRING
        memcpy(msg->data, udpRM->data, DATALEN);
    }

    return msg;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int udpSocket, listenSocket, portNr, newTCPsock;
    int err, flag = 1;
    struct sockaddr_in servAddr, newSubAddr, udpAddr;
    socklen_t subLen = sizeof(newSubAddr);
    socklen_t udpLen = sizeof(udpAddr);
    map<string, struct subContent*> subscribers;    // dictionar intre ID-ul unui subscriber si datele sale specifice
    map<string, set<string>> topics;                // dictionar intre numele unui topic si un vector de ID-uri de subs
    string commBuf;
    bool running = true;
    char newID[IDLEN], tcpBuffer[TCPBUFLEN], udpBuffer[UDPBUFLEN];
    struct tcpToServerMsg *tcpRM;
    struct udpToServerMsg *udpRM;
    struct serverToTcpMsg *toSend;

    DIE(argc != 2, "usage: ./server <PORT>\n");

    portNr = atoi(argv[1]);
    DIE(portNr == 0, "atoi(portNr) err\n");

    fd_set readFds;	    // multimea de citire folosita in select()
	fd_set tmpFds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea readFds

    // se goleste multimea de descriptori de citire (readFds) si multimea temporara (tmpFds)
	FD_ZERO(&readFds);
	FD_ZERO(&tmpFds);

    // se adauga stdin in multimea de socketi pe care se asculta
    FD_SET(STDIN_FILENO, &readFds);

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenSocket < 0, "listenSocket error\n");

    // ca sa pot refolosi acelasi port pt listen
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed\n");
    }

    // setare adresa
    memset((char *) &servAddr, 0, sizeof(servAddr));
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(portNr);

    // bind listenSocket
    err = bind(listenSocket, (struct sockaddr *) &servAddr, sizeof(struct sockaddr));
    DIE(err < 0, "error at listenSocket bind\n");

    // dezactivare Neagle
    err = setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(err < 0, "error at Neagle disable\n");

    // listen
    err = listen(listenSocket, MAXBACKLOGCLN);
    DIE(err < 0, "error at listen");

    // se adauga listenSocket in multimea de socketi pe care se asculta
    FD_SET(listenSocket, &readFds);
    fdmax = listenSocket;

    // bind udpSocket
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpSocket < 0, "udpSocket error\n");
    err = bind(udpSocket, (struct sockaddr *) &servAddr, sizeof(struct sockaddr));
    DIE(err < 0, "error at udpSocket bind\n");

    // se adauga udpSocket in multimea de socketi pe care se asculta
    FD_SET(udpSocket, &readFds);

    if (udpSocket > fdmax) {
        fdmax = udpSocket;
    }

    while (running) {
        tmpFds = readFds;
        err = select(fdmax + 1, &tmpFds, NULL, NULL, NULL);
        DIE(err < 0, "select error\n");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmpFds)) {
                if (i == STDIN_FILENO) {
                    // se citeste de la tastatura
                    getline(cin, commBuf);

                    if (commBuf.compare("exit") != 0) {
                        printf("Unknown command\n");
                    } else {
                        // se inchide serverul

                        // se inchid toti clientii
                        for (auto &sub : subscribers) {
                            if (sub.second->online) {
                                shutdown(sub.second->sockfd, SHUT_RDWR);
                                close(sub.second->sockfd);
                            }
                        }

                        // apoi restul de socketi
                        shutdown(listenSocket, SHUT_RDWR);
                        shutdown(STDIN_FILENO, SHUT_RDWR);
                        shutdown(udpSocket, SHUT_RDWR);
                        close(listenSocket);
                        close(STDIN_FILENO);
                        close(udpSocket);

                        running = false;
                    }

                } else if (i == listenSocket) {
                    // a venit o cerere de conectare
                    newTCPsock = accept(listenSocket, (struct sockaddr *) &newSubAddr, &subLen);
                    DIE(newTCPsock < 0, "error at accepting new TCP socket\n");

                    // receptioneaza ID-ul
                    memset(newID, 0, IDLEN);
                    err = recv(newTCPsock, newID, sizeof(newID), 0);
                    DIE(err < 0, "error at receiveing newID\n");

                    // verifica ID
                    bool isNewSub = true;

                    if (subscribers.contains(newID)) {
                        // daca se gaseste un subscriber cu acelasi ID
                        isNewSub = false;

                        if (subscribers[newID]->online == true) {
                            // care este online
                            printf("Client %s already connected.\n", newID);
                            shutdown(newTCPsock, SHUT_RDWR);
                            close(newTCPsock);
                        } else {
                            // care este offline
                            FD_SET(newTCPsock, &readFds);
                            subscribers[newID]->sockfd = newTCPsock;
                            subscribers[newID]->online = true;

                            if (newTCPsock > fdmax) {
                                fdmax = newTCPsock;
                            }

                            printf("New client %s connected from %s:%d.\n",
                                    newID, inet_ntoa(newSubAddr.sin_addr), ntohs(newSubAddr.sin_port));

                            // dezactivare Neagle
                            err = setsockopt(newTCPsock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                            DIE(err < 0, "error at Neagle disable\n");

                            // trimite mesajele receptionate cat timp a fost online
                            for (auto &msg : subscribers[newID]->oldMsg) {
                                err = send(newTCPsock, msg, sizeof(struct serverToTcpMsg), 0);
                                DIE(err < 0, "error at sending oldMsg\n");

                                free(msg);
                            }

                            subscribers[newID]->oldMsg.clear();
                        }
                    }

                    // este un subscriber nou
                    if (isNewSub) {
                        // se adauga noul socket si se adauga in mapul de subscriberi
                        FD_SET(newTCPsock, &readFds);

                        if (newTCPsock > fdmax) {
                            fdmax = newTCPsock;
                        }

                        // dezactivare Neagle
                        err = setsockopt(newTCPsock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                        DIE(err < 0, "error at Neagle disable\n");

                        subscribers.insert(pair<string, subContent*>(newID, new subContent(newTCPsock)));
                        printf("New client %s connected from %s:%d.\n",
                                newID, inet_ntoa(newSubAddr.sin_addr), ntohs(newSubAddr.sin_port));
                    }
                } else if (i == udpSocket) {
                    // a primit mesaj de la un client UDP
                    memset(udpBuffer, 0, UDPBUFLEN);
                    err = recvfrom(udpSocket, udpBuffer, UDPBUFLEN, 0, (struct sockaddr*) &udpAddr, &udpLen);
                    DIE(err < 0, "recv from udp\n");

                    udpRM = (struct udpToServerMsg*)udpBuffer;
                    DIE(udpRM->dataType > 3, "unknown dataType from udpMsg\n");

                    // verificare topic
                    if (topics.contains(udpRM->topic)) {
                        // daca exista deja topicul
                        for (auto &subID : topics[udpRM->topic]) {
                            // daca subscriberul este online
                            if (subscribers[subID]->online) {
                                toSend = parseMsg(udpRM, udpAddr);

                                // trimite acum mesajul
                                err = send(subscribers[subID]->sockfd, (char *)toSend,
                                            sizeof(struct serverToTcpMsg), 0);
                                DIE(err < 0, "error at sending to tcp client\n");

                                free(toSend);
                            } else if ((!subscribers[subID]->online)
                                        && subscribers[subID]->subscribedTopics[udpRM->topic] == 1) {
                                // daca e offline, dar este abonat cu optiunea SF
                                toSend = parseMsg(udpRM, udpAddr);

                                // pastreaza mesajul pentru a fi trimis cand subscriberul se reconecteaza
                                subscribers[subID]->oldMsg.push_back(toSend);
                            }
                        }
                    } // daca topicul nu exita, mesajul se ignora
                } else {
                    // serverul a primit un mesaj de la un client TCP
                    memset(tcpBuffer, 0, TCPBUFLEN);
                    err = recv(i, tcpBuffer, sizeof(struct tcpToServerMsg), 0);
                    DIE(err < 0, "recv from tcp\n");

                    if (err == 0) {
                        // s-a inchis un client TCP
                        for (auto &sub : subscribers) {
                            if (i == sub.second->sockfd) {
                                sub.second->online = false;
                                sub.second->sockfd = -1;
                                printf("Client %s disconnected.\n", sub.first.c_str());
                                shutdown(i, SHUT_RDWR);
                                close(i);
                                FD_CLR(i, &readFds);
                                break;
                            }
                        }
                    } else {
                        // s-a primit o comanda de subscribe/unsubcribe de la clientul TCP
                        tcpRM = (struct tcpToServerMsg*)tcpBuffer;

                        if (tcpRM->sf == 2) {
                            // sf == 2 => clientul s-a dezabonat de la acel topic
                            topics[tcpRM->topic].erase(tcpRM->ID);
                            subscribers[tcpRM->ID]->subscribedTopics.erase(tcpRM->topic);
                        } else {
                            // clientul se aboneaza la topic
                            topics[tcpRM->topic].insert(tcpRM->ID);
                            subscribers[tcpRM->ID]->subscribedTopics[tcpRM->topic] = tcpRM->sf;
                        }
                    }
                }
            }
        }
    }

    // eliberare memorie
    for (auto &x : subscribers) {
        if (!x.second->oldMsg.empty()) {
            for (auto &msg : x.second->oldMsg) {
                free(msg);
            }
        }

        delete x.second;
    }

    return 0;
}
