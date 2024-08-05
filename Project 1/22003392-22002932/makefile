all: comcli comserver

comserver: comserver.c
	gcc -Wall -o comserver comserver.c -lrt

comcli: comcli.c
	gcc -Wall -o comcli comcli.c -lrt

clean:
	rm -fr *~ comcli comserver