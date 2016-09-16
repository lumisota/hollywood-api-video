CC=clang
CPLUS=clang++
BIN=./bin
SOURCE=./
PROG=sender receiver file-sender file-receiver 
LIST=$(addprefix $(BIN)/, $(PROG))

all: $(LIST) 

$(BIN)/%: $(SOURCE)%.c lib/cobs.c lib/hollywood.c | $(BIN)
	$(CC) -lm $< -o $@ lib/cobs.c lib/hollywood.c


$(BIN):
	mkdir $@

clean:
	rm $(LIST) $(BIN)/vdo-sender

