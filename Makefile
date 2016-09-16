CC=clang
CPLUS=clang++
BIN=./bin
SOURCE=./
PROG=sender receiver file-sender file-receiver 
LIST=$(addprefix $(BIN)/, $(PROG))

all: $(LIST) $(BIN)/vdo-sender

$(BIN)/%: $(SOURCE)%.c lib/cobs.c lib/hollywood.c | $(BIN)
	$(CC) -lm $< -o $@ lib/cobs.c lib/hollywood.c

$(BIN)/vdo-sender: $(SOURCE)vdo-sender.cpp lib/cobs.c lib/hollywood.c vdosrc/mpeg.c vdosrc/mpeg_a.c vdosrc/mpeg.h | $(BIN)
	$(CPLUS) -lm -o vdo-sender $(SOURCE)vdo-sender.cpp lib/cobs.c lib/hollywood.c vdosrc/mpeg.c vdosrc/mpeg_a.c vdosrc/mpeg.h 

$(BIN):
	mkdir $@

clean:
	rm $(LIST) $(BIN)/vdo-sender

