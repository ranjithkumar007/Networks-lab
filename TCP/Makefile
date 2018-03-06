CC = g++
CFLAGS = -w

a.out : Server/tcpserver Client/tcpclient
	@echo "Compilation Successful"
	@echo "run server using cd Server; ./tcpserver port"
	@echo "run client using cd Client; ./tcpclient host port filename"
	@echo "see Client folder for sample inputs."

tcpserver: Server/tcpserver.cpp
	$(CC) $(CFLAGS) -o Server/tcpserver Server/tcpserver.cpp

tcpclient: Client/tcpclient.cpp
	$(CC) $(CFLAGS) -o Client/tcpclient Client/tcpclient.cpp

clean:
	rm Client/tcpclient Server/tcpserver

# to gen random large text files : base64 /dev/urandom | head -c 100000000 >> file.txt 
