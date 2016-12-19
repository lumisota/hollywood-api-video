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
 * TCP Hollywood - Example Receiver
 */

#include "lib/hollywood.h"
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* from https://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html */
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y) {
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

int main(int argc, char *argv[]) {
	hlywd_sock h_sock;
	int fd, cfd;
	socklen_t cfd_len;
	struct sockaddr_in server_addr, client_addr;
	char buffer[1000];
	ssize_t read_len;
	uint8_t substream_id;
	struct timeval *elapsed[6000] = {NULL};

	if (argc < 2) {
		printf("Usage: receiver [1|0]\n");
		return 1;
	}

	/* Create a socket */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Unable to create socket\n");
		return 1;
	}

	/* Bind */
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8882);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
		printf("Unable to bind\n");
		return 3;
	}

	/* Listen */
	if (listen(fd, 1) == -1) {
		printf("Unable to listen\n");
		return 4;
	}

	/* Accept a connection */
	cfd_len = sizeof(struct sockaddr);
	if ((cfd = accept(fd, (struct sockaddr *) &client_addr, &cfd_len)) == -1) {
		printf("Unable to accept connection\n");
		return 5;
	}

	/* Create a Hollywood socket */
	int oo = atoi(argv[1]);
	if (hollywood_socket(cfd, &h_sock, oo) != 0) {
		printf("Unable to create Hollywood socket\n");
		return 2;
	}

	/* Receive message loop */
	while ((read_len = recv_message(&h_sock, buffer, 1000, 0, &substream_id)) > 0) {
		struct timeval send_time, recv_time;
		int message_num = 0;
		memcpy(&message_num, buffer, sizeof(int));
		memcpy(&send_time, (buffer+sizeof(int)), sizeof(struct timeval));
		gettimeofday(&recv_time, NULL);
		printf("%ld.%06ld %d\n", recv_time.tv_sec, recv_time.tv_usec, message_num);
		if (elapsed[message_num] == NULL) {
			elapsed[message_num] = (struct timeval *) malloc(sizeof(struct timeval));
			timeval_subtract(elapsed[message_num], &recv_time, &send_time);
		}
	}

	/* Close connection */
	close(cfd);
	close(fd);

	for (int i = 0; i < 6000; i++) {
		printf("%d %ld.%06ld\n", i, elapsed[i]->tv_sec, elapsed[i]->tv_usec);
		free(elapsed[i]);
	}

	return 0;
}
