CC = gcc
CFLAGS = -Wall -g

all: producer consumer

producer: producer.c mf.c
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lpthread

consumer: consumer.c mf.c
	$(CC) $(CFLAGS) -o $@ $^ -lrt -lpthread

clean:
	rm -f producer consumer
