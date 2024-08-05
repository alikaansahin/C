CC = gcc
CFLAGS = -Wall -Wextra -pedantic

all: comserver comcli

comserver: comserver.c
	$(CC) $(CFLAGS) comserver.c -o comserver -lrt

comcli: comcli.c
	$(CC) $(CFLAGS) comcli.c -o comcli -lrt

clean:
	rm -f comserver comcli
