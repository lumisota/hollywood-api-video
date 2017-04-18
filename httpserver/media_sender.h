//
//  media_sender.h
//  
//
//  Created by Saba Ahsan on 17/02/17.
//
//

#ifndef ____media_sender__
#define ____media_sender__

#include "../lib/hollywood.h"
#include "../common/http_ops.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#define MAXQLEN 10 

/* Only one message is passed to queue at a time
 next message will only be parsed once this message is sent
 */
struct hlywd_message{
    unsigned char message[HOLLYWOOD_MSG_SIZE];
    uint64_t msg_size;
    uint8_t stream_complete;
    int depends_on;
    int framing_ms;
    int lifetime_ms;
    struct hlywd_message * next;
};

struct hlywd_attr {
    int seq;
    int qlen; /*number of messages waiting to be sent*/
    hlywd_sock * hlywd_socket;
    uint8_t file_complete;
    struct hlywd_message * hlywd_msg;
};

struct parse_attr {
    FILE * fptr;
    struct hlywd_attr *hlywd_data;
};

int send_media_over_hollywood(hlywd_sock * sock, FILE * fptr, int seq, char *src_filename);

/* function that parses an mp4 file and creates hollywood messages
	it is called as a thread. and parse_attr is to be passed as argument */
void * fill_timing_info(void * a);

/*  writes the messages to the intermediary hollywood layer */
void * write_to_hollywood(void * );

/*initialize the socket from the hostname, does not initialize hollywood*/
int initialize_socket(const char * hostname);



#endif /* defined(____media_sender__) */
