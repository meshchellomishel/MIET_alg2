CC=gcc
INC=/usr/src/kernels/6.10.9-200.fc40.x86_64/tools/include/
FLAGS=-Wall -ggdb3

lab1: lab1.c Makefile
	$(CC) -I$(INC) $(FLAGS) lab1.c -o lab1 -lm

clean:
	rm lab1

.PHONY=clean
