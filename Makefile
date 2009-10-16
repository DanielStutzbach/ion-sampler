OPT=-O3
CFLAGS=-g -Wall -W --std=gnu99 $(OPT)
CC=gcc
LDFLAGS=-lrt

gnutella: gnutella.c heap.c common.c queue.c

clean:
	rm -f gnutella *.o