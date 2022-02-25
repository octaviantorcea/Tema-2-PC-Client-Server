PORTNO = 12345

subscriber: subscriber.cpp
	g++ -std=c++2a -Wall subscriber.cpp -o subscriber

server: server.cpp
	g++ -std=c++2a -Wall server.cpp -o server

run-server: server
	./server ${PORTNO}

clean:
	rm -rf server subscriber
