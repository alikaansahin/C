CC = gcc
CFLAGS = -Wall -g

all: mfserver client

mfserver: mfserver.c mf.c
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lpthread

client: client.c mf.c
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lpthread

clean:
	rm -f mfserver client
