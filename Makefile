CC=gcc
CPP=g++
CCFLAGS=-std=gnu99 -c -g -Wall -I/Users/sahsan/Documents/libraries/include `xml2-config --cflags`
CPPFLAGS=-std=c++11 -c -g -Wall -I/Users/sahsan/Documents/libraries/include `xml2-config --cflags`
LDFLAGS=-L/Users/sahsan/Documents/libraries/lib -lm -lpthread -lavformat -lavcodec -lavutil -lswresample `xml2-config --libs` -lz -ldl 

SERVER_DIR=httpserver
SERVER_OBJ=media_sender.o httpd.o tsdemux.o
SERVER_SRC=media_sender.c httpd.c tsdemux.c
SERVER_HDR=tsdemux.h media_sender.h

SERVER_TL_DIR=timeless_server
SERVER_TL_OBJ=media_sender_timeless.o httpd_timeless.o 
SERVER_TL_SRC=media_sender_timeless.c httpd_timeless.c 
SERVER_TL_HDR=media_sender_timeless.h

CLIENT_DIR=httpclient
CLIENT_OBJ=mm_parser.o httpc.o playout_buffer.o readmpd.o bola.o panda.o mm_download.o
#CLIENT_SRC=mm_parser.c httpc.c playout_buffer.c readmpd.c mm_download.cpp bola.c panda.c AdaptationManager.cpp AdaptationManagerABMAplus.cpp
CLIENT_HDR=mm_parser.h playout_buffer.h readmpd.h mm_download.h bola.h panda.h AdaptationManager.h AdaptationManagerABMAplus.h
CLIENT_CPP_OBJ=AdaptationManager.o AdaptationManagerABMAplus.o

COMMON_DIR=common
COMMON_OBJ=http_ops.o helper.o
COMMON_SRC=http_ops.c helper.c 
COMMON_HDR=http_ops.h helper.h

LIB_SRC=hollywood.c cobs.c
LIB_HDR=hollywood.h cobs.h
LIB_OBJ=hollywood.o cobs.o
LIB_DIR=lib

all: httpc httptl #httpd

clean:
	rm httpd httpc httptl $(SERVER_TL_OBJ) $(SERVER_OBJ) $(LIB_OBJ) $(CLIENT_OBJ) $(COMMON_OBJ) $(CLIENT_CPP_OBJ) 

testfiles:
	curl http://www.netlab.tkk.fi/tutkimus/rtc/BBB_8bitrates_hd.tar.gz > BBB_8bitrates_hd.tar.gz
	tar -zxvf BBB_8bitrates_hd.tar.gz
	mkdir testfiles
	mv BBB_8bitrates_hd testfiles/.

$(COMMON_OBJ): $(patsubst %,$(COMMON_DIR)/%, $(COMMON_HDR))
	$(CC) $(CCFLAGS) $(COMMON_DIR)/$*.c -o $*.o 


#$(SERVER_OBJ): $(patsubst %,$(SERVER_DIR)/%, $(SERVER_HDR))
#	$(CC) $(CCFLAGS) $(SERVER_DIR)/$*.c -o $*.o 

$(SERVER_TL_OBJ): $(patsubst %,$(SERVER_TL_DIR)/%, $(SERVER_TL_HDR))
	$(CC) $(CCFLAGS) $(SERVER_TL_DIR)/$*.c -o $*.o 

$(CLIENT_OBJ): $(patsubst %,$(CLIENT_DIR)/%, $(CLIENT_HDR))
	$(CPP) $(CPPFLAGS) $(CLIENT_DIR)/$*.c -o $*.o 

$(CLIENT_CPP_OBJ): $(patsubst %,$(CLIENT_DIR)/%, $(CLIENT_HDR))
	$(CPP) $(CPPFLAGS) $(CLIENT_DIR)/$*.cpp -o $*.o 

$(LIB_OBJ): $(patsubst %,$(LIB_DIR)/%, $(LIB_HDR)) 
	$(CC) $(CCFLAGS) -x c $(LIB_DIR)/$*.c -o $*.o 


#httpd: $(COMMON_OBJ) $(SERVER_OBJ) $(LIB_OBJ) testfiles
#	$(CC) -o httpd $(SERVER_OBJ) $(COMMON_OBJ) $(LIB_OBJ) $(LDFLAGS) 

httptl: $(COMMON_OBJ) $(SERVER_TL_OBJ) $(LIB_OBJ) testfiles
	$(CC) -o httptl $(SERVER_TL_OBJ) $(COMMON_OBJ) $(LIB_OBJ) $(LDFLAGS) 

httpc: $(COMMON_OBJ) $(CLIENT_CPP_OBJ) $(CLIENT_OBJ) $(LIB_OBJ) testfiles
	$(CPP) -o httpc $(CLIENT_OBJ) $(COMMON_OBJ) $(CLIENT_CPP_OBJ) $(LIB_OBJ) $(LDFLAGS) 

