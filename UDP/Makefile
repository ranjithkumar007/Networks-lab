CC = gcc
CFLAGS = -w

a.out : Server/udpserver Client/udpclient
	@echo "Compilation Successful"
	@echo "run server using cd Server; ./udpserver port"
	@echo "run client using cd Client; ./udpclient host port filename"
	@echo "see Client folder for sample inputs."

udpserver: Server/udpserver.c
	$(CC) $(CFLAGS) -o Server/udpserver Server/udpserver.c

udpclient: Client/udpclient.c
	$(CC) $(CFLAGS) -o Client/udpclient Client/udpclient.c

clean:
	rm Client/udpclient Server/udpserver

# to gen random large text files : base64 /dev/urandom | head -c 100000000 >> file.txt 
