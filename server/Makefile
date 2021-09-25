CC=${CROSS_COMPILE}gcc

all: default

default: aesdsocket

writer: aesdsocket.c
	$(CC) -c -o aesdsocket.o aesdsocket.c
	$(CC) -o aesdsocket aesdsocket.o


clean:
	rm -f aesdsocket aesdsocket.o

.PHONY: clean
