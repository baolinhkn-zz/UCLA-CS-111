CC=gcc
CFLAGS=-g -Wall -Wextra
TARGET=lab1a

default:
	@$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	@rm -f $(TARGET)
	@rm lab1a-104732121.tar.gz

dist:
	@tar -czf lab1a-104732121.tar.gz README Makefile $(TARGET).c
