.PHONY: all default clean

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) $(LDFLAGS) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CC) $(CCFLAGS) -c -o aesdsocket.o aesdsocket.c

clean:
	-rm aesdsocket *.o