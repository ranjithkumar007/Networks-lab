CC = gcc
CFLAGS = -w
SPORT = 6000

a.out : Server/tcpserver Client/tcpclient
	@echo "Compilation Successful"
	@echo "run server using cd Server; ./tcpserver port"
	@echo "run client using cd Client; ./tcpclient host port filename"
	@echo "see Client folder for sample inputs."

tcpserver: Server/tcpserver.c
	$(CC) $(CFLAGS) -o Server/tcpserver Server/tcpserver.c

tcpclient: Client/tcpclient.c
	$(CC) $(CFLAGS) -o Client/tcpclient Client/tcpclient.c

clean:
	rm Client/tcpclient Server/tcpserver

# to gen random large text files : base64 /dev/urandom | head -c 100000000 >> file.txt 
