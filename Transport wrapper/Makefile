CXX = g++
CFLAGS = -std=c++11

a.out : 				client server
						@echo "Compilation successful"

client : 				test-client.cpp librhnet.a
						$(CXX) $(CFLAGS) -g -o client test-client.cpp librhnet.a -pthread

server : 				test-server.cpp librhnet.a
						$(CXX) $(CFLAGS) -g -o server test-server.cpp librhnet.a -pthread

librhnet.a :			transport-wrapper.o
						ar rcs librhnet.a transport-wrapper.o

transport-wrapper.o : 	transport-wrapper.cpp transport-wrapper.h
						$(CXX) $(CFLAGS) -g -o transport-wrapper.o -c transport-wrapper.cpp -pthread

clean :		
						rm librhnet.a client server transport-wrapper.o server-output
