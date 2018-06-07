CC=gcc
CFLAGS=-g -Wall -Wextra -lmraa -lm -lssl -lcrypto
TCP_LAB=lab4c_tcp
TLS_LAB=lab4c_tls

default:
	@$(CC) $(CFLAGS) -o $(TCP_LAB) $(TCP_LAB).c
	@$(CC) $(CFLAGS) -o $(TLS_LAB) $(TLS_LAB).c

clean:
	@rm -f $(TCP_LAB) $(TLS_LAB) lab4c-104732121.tar.gz

dist:
	@tar -czf lab4c-104732121.tar.gz README Makefile $(TCP_LAB).c $(TLS_LAB).c
