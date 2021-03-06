all: server client

server: server.cpp
	c++ server.cpp -o server -libverbs -lrdmacm -std=c++11 -std=gnu++11

client: client.cpp
	c++ client.cpp -o client -libverbs -lrdmacm -std=c++11 -std=gnu++11

clean:
	rm server client