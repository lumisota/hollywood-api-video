/*
 * Copyright (c) 2016 University of Glasgow
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <organization> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * TCP Hollywood - Example Sender
 */

#include "lib/hollywood.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

void timespec_cmp(struct timespec *a, struct timespec *b, struct timespec *res) {
    if ((b->tv_nsec - a->tv_nsec) < 0) {
        res->tv_sec = b->tv_sec - a->tv_sec - 1;
        res->tv_nsec = b->tv_nsec - a->tv_nsec + 1000000000;
    } else {
        res->tv_sec = b->tv_sec - a->tv_sec;
        res->tv_nsec = b->tv_nsec - a->tv_nsec;
    }
}

int main(int argc, char *argv[]) {
	struct addrinfo hints, *serveraddr;
	hlywd_sock hlywd_socket;
	int fd, i, msg_len;
	char *buffer = malloc(200);

	/* Check for hostname parameter */
	if (argc != 4) {
		printf("Usage: %s <hostname> <playout> [0|1]\n", argv[0]);
		return 1;
	}

	/* Lookup hostname */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(argv[1], "8882", &hints, &serveraddr) != 0) {
		printf("Hostname lookup failed\n");
		return 2;
	}

    /* Create a socket */
	if ((fd = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol)) == -1) {
		printf("Unable to create socket\n");
		return 3;
	}

	/* Connect to the receiver */
	if (connect(fd, serveraddr->ai_addr, serveraddr->ai_addrlen) != 0) {
		printf("Unable to connect to receiver\n");
		close(fd);
		return 4;
	}

	/* Create Hollywood socket */
	if (hollywood_socket(fd, &hlywd_socket, 1, atoi(argv[3])) != 0) {
		printf("Unable to create Hollywood socket\n");
		close(fd);
		return 5;
	}

	/* Set the playout delay */
	set_playout_delay(&hlywd_socket, atoi(argv[2]));

    struct timespec ts_prev;
    struct timespec ts_now;
    struct timespec ts_diff;
    int prev_set = 0;
	clock_gettime(CLOCK_MONOTONIC, &ts_now);

    uint64_t target = 0;
    uint64_t last_sleep = 0;
    
	/* Send 6000 messages */
	for (i = 0; i < 6000; i++) {
        /* sleep duration */
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
    	if (prev_set == 0) {
    	    ts_prev.tv_sec = ts_now.tv_sec;
            ts_prev.tv_nsec = ts_now.tv_nsec;
            prev_set = 1;
            timespec_cmp(&ts_prev, &ts_now, &ts_diff);
        } else {
            timespec_cmp(&ts_prev, &ts_now, &ts_diff);
            last_sleep = (ts_diff.tv_sec*1000000 + ts_diff.tv_nsec/1000);
        }
        ts_prev.tv_sec = ts_now.tv_sec;
        ts_prev.tv_nsec = ts_now.tv_nsec;
        target = 20000 - (last_sleep-target);
        /* sleep */
		usleep(target);
        memcpy(buffer, &i, sizeof(int));
		gettimeofday((struct timeval *) (buffer+sizeof(int)), NULL);
		msg_len = send_message_time(&hlywd_socket, buffer, 160, 0, i, i, 150);
		printf("[%lld.%.9ld] Sending message number %d (length: %d)..\n", (long long) ts_now.tv_sec, ts_now.tv_nsec, i, msg_len);
		if (msg_len == -1) {
			printf("Unable to send message\n");
			free(buffer);
			close(fd);
			return 6;
		}
	}

	/* Free message buffer */
	free(buffer);

	/* Close socket */
	close(fd);

	return 0;
}

uint64_t timespec_u(struct timespec *ts) {
    return (useconds_t) 1000*ts->tv_nsec + ts->tv_sec/1000000;
}
