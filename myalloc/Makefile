CC=gcc

all: myalloc test

myalloc: myalloc.c
	$(CC) -fPIC -shared myalloc.c -o myalloc.so

test: test.c
	$(CC) test.c -o test

clean:
	rm myalloc.so test
