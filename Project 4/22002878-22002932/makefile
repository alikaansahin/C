CC = gcc
CFLAGS = -Wall -Wextra -g
DEPS = fatmod.c
OBJ = fatmod.o
EXEC = fatmod

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(OBJ) $(EXEC)
