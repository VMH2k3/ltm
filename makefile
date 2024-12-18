CC = gcc
CFLAGS = `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4`

all: client server

client: client.c
	$(CC) -o client client.c $(CFLAGS) $(LIBS)

server: server.c
	$(CC) -o server server.c

clean:
	rm -f client server
