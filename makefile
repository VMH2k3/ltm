CC = gcc
CFLAGS = `pkg-config --cflags gtk4`
LIBS_CLIENT = `pkg-config --libs gtk4`
LIBS_SERVER = -lsqlite3

all: client server

client: client.c
	$(CC) -o client client.c $(CFLAGS) $(LIBS_CLIENT)

server: server.c
	$(CC) -o server server.c $(LIBS_SERVER)

clean:
	rm -f client server
