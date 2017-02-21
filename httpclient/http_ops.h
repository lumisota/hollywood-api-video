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
#include <netdb.h>

#define MAXURLLENGTH 512
#define MAXREQUESTLENGTH 1024
#define MAXHOSTLEN 128

int get_content_length( char * buf);

int get_html_headers(int sock, char *buf, int size, uint8_t hollywood);

//int receive_response(int fd, struct metrics * metric, uint8_t hollywood);

int send_get_request(int fd, char * url, uint8_t hollywood);

int separate_host_and_filepath(char * url, char * host, char * path);

int write_to_memory (int sockfd, char * memory, int contentlen, uint8_t hollywood);


#endif /* defined(____http_ops__) */
