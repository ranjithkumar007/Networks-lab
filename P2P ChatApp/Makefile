CC = g++
CFLAGS = -std=c++11

a.out : server
	@echo "Compilation Successful"

server: server.cpp server.h
	$(CC) $(CFLAGS) -o server server.cpp

clean:
	rm server
