//
//  http_ops.h
//  
//
//  Created by Saba Ahsan on 21/02/17.
//
//

#ifndef ____http_ops__
#define ____http_ops__

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <netdb.h>
#include "../lib/hollywood.h"

#define MAXURLLENGTH 512
#define HTTPHEADERLEN 2048
#define MAXHOSTLEN 128
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define PAGESIZE 500000


int get_content_length( char * buf);

int get_html_headers(void * sock, char *buf, int size, uint8_t hollywood);

//int receive_response(int fd, struct metrics * metric, uint8_t hollywood);

int send_get_request(void * sock, char * url, uint8_t hollywood);

int separate_host_and_filepath(char * url, char * host, char * path);

int read_to_memory (void * sock, char * memory, int contentlen, uint8_t hollywood);

int connect_tcp_port (char * host, char * port);

int send_resp_headers(void * sock , const char *filename, uint8_t hollywood);

int cat(void * sock, FILE * fptr, uint8_t hollywood);

int not_found(void * sock, uint8_t hollywood);

int unimplemented(void * sock, uint8_t hollywood);


#endif /* defined(____http_ops__) */