CC=g++
CCFLAGS=-c -g -DCORO_SJLJ -I/usr/local/include
LDFLAGS=-L/usr/local/lib -lpthread

ALL_OBJ=helper.o vdo-sender.o httpd.o mpeg.o mpeg_a.o 
ALL_SRC=helper.c vdo-sender.c httpd.c  mpeg.c mpeg_a.c 
ALL_HDR=helper.h vdo-sender.h mpeg.h mpeg_a.h 
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


