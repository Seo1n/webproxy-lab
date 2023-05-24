CC=gcc
CFLAGS=-g -O1 -Wall
LDLIBS=-lpthread

all: echoclient echoserveri 

echoclient: echoclient.c csapp.h csapp.c
echoserveri: echoserveri.c csapp.h csapp.c

clean:
	rm -f *.o *~ *.exe echoclient echoserveri csapp.o