CC = $(CROSS_COMPILE)gcc
CFLAGS += -g -Wall -Werror

.PHONY: all default clean

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c -o aesdsocket.o aesdsocket.c

clean:
	-rm aesdsocket *.o