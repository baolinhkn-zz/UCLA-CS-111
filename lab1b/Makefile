CC=gcc
CFLAGS=-g -Wall -Wextra -lz
CLIENT=lab1b-client
SERVER=lab1b-server

default:
	@$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT).c
	@$(CC) $(CFLAGS) -o $(SERVER) $(SERVER).c

client:
	@$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT).c

server:
	@$(CC) $(CFLAGS) -o $(SERVER) $(SERVER).c

clean:
	@rm -f $(CLIENT) $(SERVER)
	@rm lab1b-104732121.tar.gz

dist:
	@tar -czf lab1b-104732121.tar.gz README Makefile $(CLIENT).c $(SERVER).c
