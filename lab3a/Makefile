CC=gcc
CFLAGS=-g -Wall -Wextra

default: 
	@$(CC) $(CFLAGS) -o lab3a lab3a.c
clean:
	@rm -f lab3a lab3a-104732121.tar.gz

dist:
	@@tar -czf lab3a-104732121.tar.gz README Makefile lab3a.c ext2_fs.h
