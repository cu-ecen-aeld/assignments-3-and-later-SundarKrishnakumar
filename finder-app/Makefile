CC=gcc
XCC=aarch64-none-linux-gnu-gcc
MATCHSTR=aarch64-none-linux-gnu-

default: writer

writer: writer.c
ifeq ($(CROSS_COMPILE), $(MATCHSTR))
	$(XCC) -c -o writer.o writer.c
	$(XCC) -o writer writer.o
else
	$(CC) -c -o writer.o writer.c
	$(CC) -o writer writer.o
endif

clean:
	rm -f writer writer.o

.PHONY: clean
