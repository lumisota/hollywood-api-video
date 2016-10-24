/*vdo-sender.h*/



#include "../lib/hollywood.h"
#include "mpeg.h"
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


#define MAXQLEN 10 /*Maximum number of messages waiting to be sent*/





struct hlywd_attr {
	int seq; 
	int qlen; /*number of messages waiting to be sent*/ 
	hlywd_sock hlywd_socket;
	bool file_complete; 
	struct hlywd_message * hlywd_msg;  
};

struct parse_attr {
	FILE * fptr;
	struct mp4_i m;
	struct hlywd_attr * h; 
};


/* function that parses an mp4 file and creates hollywood messages
	it is called as a thread. and parse_attr is to be passed as argument */
void * parse_mp4file(void * );

/* function that sends the hollywood messages, both parse_mp4file and this
	use mutexes and condition variables to control access to the message
	it is called as a thread. and parse_attr is to be passed as argument */
void * send_message(void * ); 

/*initialize the socket from the hostname, does not initialize hollywood*/
int initialize_socket(const char * hostname);

/*initialize the message structure and update pointers inside both argument parameters*/
int initalize_message(struct parse_att *, struct hlywd_attr *);

void on_exit();
