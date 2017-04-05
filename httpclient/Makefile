CC=gcc
CCFLAGS=-c -g -Wall -I/usr/local/include `xml2-config --cflags`
LDFLAGS=-L/usr/local/lib -lpthread -lavformat -lavcodec -lavutil `xml2-config --libs`


ALL_OBJ=helper.o mm_parser.o httpc.o playout_buffer.o readmpd.o ../common/http_ops.o mm_download.o bola.o
ALL_SRC=helper.c mm_parser.c httpc.c playout_buffer.c readmpd.c ../common/http_ops.c mm_download.c bola.c
ALL_HDR=helper.h mm_parser.h playout_buffer.h readmpd.h ../common/http_ops.h mm_download.h bola.h
LIB_SRC=hollywood.c cobs.c
LIB_HDR=hollywood.h cobs.h
LIB_OBJ=hollywood.o cobs.o
LIB_DIR=../lib

all: httpc

clean:
	rm httpc $(ALL_OBJ) $(LIB_OBJ)

.c.o: $(ALL_HDR)
	$(CC) $(CCFLAGS) $*.c -o $*.o 

$(LIB_OBJ): $(patsubst %,$(LIB_DIR)/%, $(LIB_HDR)) 
	$(CC) $(CCFLAGS) $(LIB_DIR)/$*.c -o $*.o 


httpc: $(ALL_OBJ) $(LIB_OBJ)
	$(CC) -lm -o httpc $(ALL_OBJ) $(LIB_OBJ) $(LDFLAGS) 


