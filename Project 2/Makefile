CC := gcc
CFLAGS := -g -Wall

TARGET := my_program

all: $(TARGET)

$(TARGET): tsl_deneme.c libtsl.a
	$(CC) $(CFLAGS) -o $@ tsl_deneme.c libtsl.a

libtsl.a: tsl_deneme.c tsl_deneme.h
	$(CC) -c $(CFLAGS) -o tsl.o tsl_deneme.c
	ar rcs $@ tsl.o

clean:
	rm -rf core *.o $(TARGET) libtsl.a
