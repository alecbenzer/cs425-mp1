all: main

main: main.c
	mkdir -p bin
	$(CC) -D_GNU_SOURCE -o bin/main main.c

.PHONY: clean
clean:
	rm -rf bin/
