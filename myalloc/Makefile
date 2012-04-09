CC=gcc

all: libmyalloc test

libmyalloc: myalloc.c
	$(CC) -fPIC -shared -g -nostdlib myalloc.c -o libmyalloc.so

test: test.c
	$(CC) test.c -o test

clean:
	rm libmyalloc.so test
