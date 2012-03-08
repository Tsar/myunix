CC=gcc
CFLAGS=-O3 -c
EXECUTABLE=myreverse

all: $(EXECUTABLE)

$(EXECUTABLE): myreverse.o
	$(CC) myreverse.o -o $@

myreverse.o: myreverse.c
	$(CC) $(CFLAGS) myreverse.c

clean:
	rm -rf *.o $(EXECUTABLE)
