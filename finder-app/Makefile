CC=${CROSS_COMPILE}gcc

default: writer

writer: writer.c
	$(CC) -c -o writer.o writer.c
	$(CC) -o writer writer.o


clean:
	rm -f writer writer.o

.PHONY: clean
