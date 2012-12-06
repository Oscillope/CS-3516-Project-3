CC = g++
CFLAGS = -Wall

all: overlay
me a sandwich: CFLAGS += -DSANDWICH
me a sandwich: debug
debug: CFLAGS += -DDEBUG
debug: overlay
overlay: overlay.o
	$(CC) $(CFLAGS) overlay.o -o overlay
overlay.o:
	$(CC) $(CFLAGS) -c overlay.cpp
clean:
	rm -f *~ *.o
