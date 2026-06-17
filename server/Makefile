CC=$(CROSS_COMPILE)gcc

.PHONY: all default clean

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CC) -c -o aesdsocket.o aesdsocket.c

clean:
	-rm aesdsocket *.o