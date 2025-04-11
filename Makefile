# Makefile for the concurrent hash table project
CC = gcc
CFLAGS = -Wall -pthread

all: chash

chash: chash.o hash_table.o
	$(CC) $(CFLAGS) -o chash chash.o hash_table.o

chash.o: chash.c hash_table.h
	$(CC) $(CFLAGS) -c chash.c

hash_table.o: hash_table.c hash_table.h
	$(CC) $(CFLAGS) -c hash_table.c

clean:
	rm -f *.o chash output.txt
