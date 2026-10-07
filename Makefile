CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -pthread -O0 -g

TARGETS = server client

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean