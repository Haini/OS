CC=gcc
DEFS=-D_XOPEN_SOURCE=500 -D_BSD_SOURCE -DENDEBUG
CFLAGS=-Wall -g -std=c99 -pedantic $(DEFS)

OBJECTFILES=server.c

.PHONY: all clean

all: server client

server: $(OBJECTFILES)
	$(CC) $(CFLAGS) -o $@ $^

client: client.c 
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f server
