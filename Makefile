CC=gcc
CFLAGS=-Wall

all: fsh

fsh: fsh.c
	$(CC) $(CFLAGS) fsh.c -o fsh

clean:
	rm -f fsh *.o
