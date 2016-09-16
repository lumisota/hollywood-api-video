CC=clang++

all: vdo-sender


vdo-sender: vdo-sender.cpp mpeg.cpp mpeg_a.cpp helper.cpp ../lib/cobs.c ../lib/hollywood.c
	$(CC) -lm -o vdo-sender vdo-sender.cpp mpeg.cpp mpeg_a.cpp helper.cpp ../lib/cobs.c ../lib/hollywood.c



clean:
	rm vdo-sender

