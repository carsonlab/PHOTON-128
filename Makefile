CC = gcc
FLAGS = -Wall

PHONY: all

all:
	$(CC) $(FLAGS) photon.c main.c -o PRESENT

clean:
	rm -rf *.o PRESENT
