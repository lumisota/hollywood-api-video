CC=clang
CPLUS=clang++

all: sender receiver file-receiver vdo-sender file-sender

clean:
	rm sender receiver vdo-sender 

sender: sender.c lib/cobs.c lib/hollywood.c
	$(CC) -g -lm -o sender sender.c lib/cobs.c lib/hollywood.c

receiver: receiver.c lib/cobs.c lib/hollywood.c
	$(CC) -g -lm -o receiver receiver.c lib/cobs.c lib/hollywood.c

vdo-sender: httpserver/vdo-sender.cpp httpserver/mpeg.cpp httpserver/mpeg_a.cpp httpserver/helper.cpp lib/cobs.c lib/hollywood.c
	$(CPLUS) -g -lm -o httpd httpserver/httpd.c httpserver/vdo-sender.cpp httpserver/mpeg.cpp httpserver/mpeg_a.cpp httpserver/helper.cpp lib/cobs.c lib/hollywood.c -lpthread


file-sender: file-sender.c lib/cobs.c lib/hollywood.c
	$(CC) -g -lm -o file-sender file-sender.c lib/cobs.c lib/hollywood.c

file-receiver: file-receiver.c lib/cobs.c lib/hollywood.c
	$(CC) -g -lm -o file-receiver file-receiver.c lib/cobs.c lib/hollywood.c


