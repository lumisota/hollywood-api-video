CC=clang

all: sender receiver file-sender file-receiver

clean:
	rm sender receiver

sender: sender.c lib/cobs.c lib/hollywood.c
	$(CC) -lm -o sender sender.c lib/cobs.c lib/hollywood.c

receiver: receiver.c lib/cobs.c lib/hollywood.c
	$(CC) -lm -o receiver receiver.c lib/cobs.c lib/hollywood.c

file-sender: file-sender.c lib/cobs.c lib/hollywood.c
	$(CC) -lm -o file-sender file-sender.c lib/cobs.c lib/hollywood.c

file-receiver: file-receiver.c lib/cobs.c lib/hollywood.c
	$(CC) -lm -o file-receiver file-receiver.c lib/cobs.c lib/hollywood.c
