all: server client

server: server.cpp
	c++ server.cpp -o server -libverbs -lrdmacm

client: client.cpp
	c++ client.cpp -o client -libverbs -lrdmacm

clean:
	rm server client