CC=clang
BIN=./bin
SOURCE=./
PROG=sender receiver file-sender file-receiver vdo-sender
LIST=$(addprefix $(BIN)/, $(PROG))

all: $(LIST)

$(BIN)/%: $(SOURCE)%.c lib/cobs.c lib/hollywood.c | $(BIN)
	$(CC) -lm $< -o $@ lib/cobs.c lib/hollywood.c

$(BIN):
	mkdir $@

clean:
	rm $(LIST)

