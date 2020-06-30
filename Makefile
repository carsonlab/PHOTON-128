CC = gcc
FLAGS = -Wall

PHONY: all

all:
	$(CC) $(FLAGS) photon.c main.c -o PHOTON

clean:
	rm -rf *.o PHOTON
