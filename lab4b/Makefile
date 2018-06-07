CC=gcc
CFLAGS=-g -Wall -Wextra -lmraa -lm
LAB=lab4b

default:
	@$(CC) $(CFLAGS) -o $(LAB) $(LAB).c

clean:
	@rm -f $(LAB) $(LAB)-104732121.tar.gz

check:
	@./test.sh

dist:
	@tar -czf lab4b-104732121.tar.gz README Makefile $(LAB).c test.sh
