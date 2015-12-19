CC=gcc
DEFS=-D_XOPEN_SOURCE=500 -D_BSD_SOURCE -DENDEBUG
CFLAGS=-Wall -g -std=c99 -pedantic $(DEFS)

.PHONY: all clean

all: server client

server: server.c
	$(CC) $(CFLAGS) -o $@ $^

client: client.c
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f server client
	
