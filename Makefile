CC=gcc

lab1: lab1.c Makefile
	$(CC) lab1.c -o lab1

clean:
	rm lab1

.PHONY=clean
