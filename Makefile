CC=gcc

all: main

main: main.c
	mkdir -p bin
	$(CC) -o bin/main main.c

.PHONY: clean
clean:
	rm -rf bin/
