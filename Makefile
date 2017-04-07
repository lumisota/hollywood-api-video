CC=gcc
CCFLAGS=-std=gnu99 -c -g -Wall -I/usr/local/include `xml2-config --cflags`
LDFLAGS=-L/usr/local/lib -lm -lpthread -lavformat -lavcodec -lavutil -lswresample `xml2-config --libs` -ldl

SERVER_DIR=httpserver
SERVER_OBJ=media_sender.o httpd.o 
SERVER_SRC=media_sender.c httpd.c 
SERVER_HDR=media_sender.h 

CLIENT_DIR=httpclient
CLIENT_OBJ=helper.o mm_parser.o httpc.o playout_buffer.o readmpd.o mm_download.o bola.o
CLIENT_SRC=helper.c mm_parser.c httpc.c playout_buffer.c readmpd.c mm_download.c bola.c
CLIENT_HDR=helper.h mm_parser.h playout_buffer.h readmpd.h mm_download.h bola.h

COMMON_DIR=common
COMMON_OBJ=http_ops.o
COMMON_SRC=http_ops.c
COMMON_HDR=http_ops.h

LIB_SRC=hollywood.c cobs.c
LIB_HDR=hollywood.h cobs.h
LIB_OBJ=hollywood.o cobs.o
LIB_DIR=lib


all: httpd httpc

clean:
	rm httpd httpc $(SERVER_OBJ) $(LIB_OBJ) $(CLIENT_OBJ) $(COMMON_OBJ)


$(COMMON_OBJ): $(patsubst %,$(COMMON_DIR)/%, $(COMMON_HDR))
	$(CC) $(CCFLAGS) $(COMMON_DIR)/$*.c -o $*.o 


$(SERVER_OBJ): $(patsubst %,$(SERVER_DIR)/%, $(SERVER_HDR))
	$(CC) $(CCFLAGS) $(SERVER_DIR)/$*.c -o $*.o 


$(CLIENT_OBJ): $(patsubst %,$(CLIENT_DIR)/%, $(CLIENT_HDR))
	$(CC) $(CCFLAGS) $(CLIENT_DIR)/$*.c -o $*.o 

$(LIB_OBJ): $(patsubst %,$(LIB_DIR)/%, $(LIB_HDR)) 
	$(CC) $(CCFLAGS) $(LIB_DIR)/$*.c -o $*.o 


httpd: $(COMMON_OBJ) $(SERVER_OBJ) $(LIB_OBJ)
	$(CC) -o httpd $(SERVER_OBJ) $(COMMON_OBJ) $(LIB_OBJ) $(LDFLAGS) 

httpc: $(COMMON_OBJ) $(CLIENT_OBJ) $(LIB_OBJ)
	$(CC) -o httpc $(CLIENT_OBJ) $(COMMON_OBJ) $(LIB_OBJ) $(LDFLAGS) 

