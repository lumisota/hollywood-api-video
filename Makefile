CC=clang

all: sender receiver

clean:
	rm sender receiver

sender: sender.c lib/cobs.c lib/hollywood.c
	$(CC) -lm -o sender sender.c lib/cobs.c lib/hollywood.c

receiver: receiver.c lib/cobs.c lib/hollywood.c
	$(CC) -lm -o receiver receiver.c lib/cobs.c lib/hollywood.c
