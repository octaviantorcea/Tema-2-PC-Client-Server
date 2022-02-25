# Tema2 PC - Aplicatie client-server

Torcea Octavian 324CA 

## Implementare client (`subscriber.cpp`)
* se adauga STDIN in multimea de citire;
* verfica daca a primit fix 3 argumente, in caz contrar se opreste;
* verifica daca ID-ul e mai lung de 10 caractere, caz in care se opreste;
* realizeaza conexiunea cu serverul (in cazul in care apare vreo eroare la
una dintre functii, clientul se opreste si afiseaza un mesaj corespunzator);
* trimite apoi imediat id-ul catre server;
* apoi intra intr-un loop "infinit" in care verifica daca a primit date de
la server sau au fost citite date de la STDIN;
* daca a fost primita o comanda de la tastatura, verifica prima data daca
aceasta este "exit", caz in care clientul se opreste;
* daca nu este "exit", va verifica daca comanda primita este de forma
"subscribe <topic> <sf>"  sau "unsubscribe <topic>"; daca nu respecta aceasta
forma, clientul va afisa la STDOUT mesajul "Unknown command." si isi va continua
executia;
* daca mesajul este unul de tip subscribe sau unsubscribe, clientul va
trimite catre server un mesaj corespunzator folosind structura tcpToServerMsg;
* daca primeste un mesaj de la server, verifica prima data daca numarul de
bytes receptionat este 0, fapt ce ar insemna ca serverul a intrerupt conexiunea
si clientul se va inchide;
* daca nr de bytes receptionat este > 0, inseamna ca a primit un mesaj valid
de la server si il va afisa conform cerintei.


## Implementare server (`server.cpp`)

Pentru a eficientiza rularea serverului, folosesc un dictionar ce face
legatura intre ID-ul unui subscriber (client) si datele sale specifice
(structura subContent), si un alt dictionar ce face legatura intre numele unui
topic si un set ce contine ID-urile clientilor ce sunt abonati la acel topic.

* Executia:
    * se verifica daca a primit numarul portului ca argument, in caz contrar se
opreste;
    * deschide un socket pentru listen si un socket pentru comunicarea cu
clientii UDP;
    * intra intr-un loop "infinit" in care va analiza de unde a primit date;
    * daca a primit date de la tastatura, verifica daca comanda primita este
`exit`, caz in care va deconecta toti clientii, va elibera memoria alocata si
executia serverului se va intrerupe. In cazul in care nu primeste comanda
`exit`, serverul va afisa la STDOUT mesajul `Unknown command.` si isi va
continua executia;
    * daca a primit date pe socketul de listen, inseamna ca a primit o cerere de
conectare de la un client TCP, va realiza conexiunea cu acesta si va receptiona
prima data ID-ul clientului;
    * daca exista deja un client care este online cu acelasi ID, va afisa un
mesaj corespunzator si va inchide comunicarea cu clientul ce tocmai a incercat
sa se conecteze;
    * daca exista deja un client cu acelasi ID, dar care nu este conectat,
adauga socket descriptorul in multimea de citire, printeaza la STDOUT mesajul
corespunzator si va trimite clientului nou conectat toate mesajele pe care
serverul le-a receptionat de la clientii UDP, ce contin topicuri la care
clientul TCP este abonat cu optiunea de store-and-forward;
    * daca este ID nou, va adauga un nou entry in dictionarul de subscriberi si
va adauga socket descriptorul in multimea de citire, afisand la STDOUT mesajul
corespunzator;
    * daca a primit un mesaj pe socketul destinat clientilor UDP, verifica prima
data daca topicul receptionat exista; daca nu exsita, serverul va ignora mesajul
primit;
    * daca topicul exista in dictionar, va incepe o verificare asupra clientilor
ce sunt abonati la acel topic; daca clientul este online, va parsa mesajul si il
va trimite imediat catre clientul TCP; daca nu este online, dar are activata
optiunea de SF, va adauga mesajul in vectorul de mesaje pe care clientul TCP le
va primi cand se va reconecta;
    * daca a primit un mesaj pe unul dintre socketii destinati clientilor TCP,
verifica prima data nr de bytes receptionati; daca este 0, inseamna ca acel
client s-a deconectat, dar nu il va scoate din dictionarul de subscriberi, ci
doar ii va actualiza starea ca fiind offline;
    * daca a receptionat un nr de bytes > 0, inseamna ca a primit o comanda de
subscribe/unsubscribe de la clientul TCP;
    * daca optiunea sf == 2, inseamna ca a primit o comanda de unsubscribe si va
sterge ID-ul clientului din setul asociat topicul, iar topicul respectiv va fi
sters din dictionarul de topicuri abonate;
    * altfel, va adauga ID-ul in setul destinat acelui topic si va introduce
topicul in dictionarul de topicuri abonate specific acestui client.


* Structuri auxiliare folosite:
    * tcpToServerMsg (`utils.h`): reprezinta structura mesajului pe care un client
TCP il trimite catre server. Contine ID-ul clientului, numele topicului la care
se aboneaza/dezaboneaza si optiunea de store-and-forward.

    * serverToTcpMsg (`utils.h`): reprezinta structura mesajului pe care serverul
il trimite catre un client. Contine numele topicului, un identificator pentru
tipul de date transmis, datele (dupa ce au fost prelucrate conform cerintei) de
la clientul UDP, IP-ul clientului UDP in forma dotted-decimal si numarul
portului clientului UDP in ordine little-endian.

    * subContent (`server.cpp`): reprezinta datele pe care serverul le retine
despre un client. Contine descriptorul de socket pe care s-a realizat
conexiunea, un indicator daca clientul este conectat sau nu, un dictionar intre
numele topicului si optiunea de SF si un vector de mesaje pe care clientul le-a
primit cat a fost offline si pe care trebuie sa le receptioneze cand se
reconecteaza.

    * udpToServerMsg (`server.cpp`): reprezinta structura mesajului pe care
serverul le receptioneaza de la un client UDP. Contine numele topicului, un
identificator pentru tipul de date transmis si datele ce trebuie furnizate catre
clientii TCP.
