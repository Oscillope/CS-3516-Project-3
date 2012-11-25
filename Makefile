CC = g++
CFLAGS = -Wall

all: router host
debug: CFLAGS += -DDEBUG
debug: router host
router: router.o
	$(CC) $(CFLAGS) router.o -o router
router.o:
	$(CC) $(CFLAGS) -c router.cpp
host: host.o
	$(CC) $(CFLAGS) host.o -o host
host.o:
	$(CC) $(CFLAGS) -c host.cpp
clean:
	rm -f *~ *.o
