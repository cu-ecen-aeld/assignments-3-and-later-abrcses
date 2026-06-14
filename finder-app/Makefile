CC=$(CROSS_COMPILE)gcc

.PHONY: all clean

all: writer

writer: writer.o
	$(CC) -o writer writer.o

writer.o: writer.c
	$(CC) -c -o writer.o writer.c

clean:
	-rm writer *.o