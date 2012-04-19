CC=gcc
CFLAGS=-O3 -c
EXECUTABLE=myropereverse

all: $(EXECUTABLE)

$(EXECUTABLE): myropereverse.o
	$(CC) myropereverse.o -o $@

myropereverse.o: myropereverse.c
	$(CC) $(CFLAGS) myropereverse.c

clean:
	rm -rf *.o $(EXECUTABLE)
