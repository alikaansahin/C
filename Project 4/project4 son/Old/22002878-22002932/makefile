# Name of the executable
EXEC = .fatnod

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

# Source files
SRCS = fatnod.c

# Object files
OBJS = $(SRCS:.c=.o)

# Rule to create the executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS)

# Rule to create object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
.PHONY: clean
clean:
	rm -f $(OBJS) $(EXEC)

# Ensure that the executable is hidden after being generated
.PHONY: hide
hide: $(EXEC)
	mv $(EXEC) .$(EXEC)

# All rule to build and hide the executable
.PHONY: all
all: $(EXEC) hide
