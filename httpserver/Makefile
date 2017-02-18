CC=gcc
CCFLAGS=-c -g -I/usr/local/include
LDFLAGS=-L/usr/local/lib -lpthread

ALL_OBJ=media_sender.o httpd.o  
ALL_SRC=media_sender.c httpd.c  
ALL_HDR=media_sender.h  
LIB_SRC=hollywood.c cobs.c
LIB_HDR=hollywood.h cobs.h
LIB_OBJ=hollywood.o cobs.o
LIB_DIR=../lib


all: httpd

clean:
	rm httpd $(ALL_OBJ) $(LIB_OBJ)

.c.o: $(ALL_HDR)
	$(CC) $(CCFLAGS) $*.c -o $*.o 


$(LIB_OBJ): $(patsubst %,$(LIB_DIR)/%, $(LIB_HDR)) 
	$(CC) $(CCFLAGS) $(LIB_DIR)/$*.c -o $*.o 


httpd: $(ALL_OBJ) $(LIB_OBJ)
	$(CC) -lm -o httpd $(ALL_OBJ) $(LIB_OBJ) $(LDFLAGS) 


