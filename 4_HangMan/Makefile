##  @file Makefile 
##  @author Constantin Schieber, e1228774
##  @brief Compiles client and server
##  @details 
##  @date

CC=gcc
DEFS=-D_XOPEN_SOURCE=500 -D_BSD_SOURCE -DENDEBUG
CFLAGS=-Wall -g -std=c99  -pedantic -pthread -lrt $(DEFS)

.PHONY: all clean

all: server client

server: server_hangman.c
	$(CC) $(CFLAGS) -o $@ $^	    

client: client_hangman.c
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f server client
