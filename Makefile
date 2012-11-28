CC = g++
CFLAGS = -Wall

all: overlay
debug: CFLAGS += -DDEBUG
debug: overlay overlay_client
overlay: overlay.o
	$(CC) $(CFLAGS) overlay.o -o overlay
overlay.o:
	$(CC) $(CFLAGS) -c overlay.cpp
clean:
	rm -f *~ *.o
