CC=gcc
CFLAGS=-Wall
TARGET=build

all: server client

server:
	mkdir -p $(TARGET)
	$(CC) $(CFLAGS) -o ${TARGET}/server src/server.c

client:
	mkdir -p $(TARGET)
	$(CC) $(CFLAGS) -o ${TARGET}/client src/client.c

clean:
	rm -r $(TARGET)

