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

int main(int argc, char *argv[]) {
	struct addrinfo hints, *serveraddr;
	hlywd_sock hlywd_socket;
	int fd, i, msg_len;
	char *buffer = (char *)malloc(200);

	/* Check for hostname parameter */
	if (argc != 2) {
		printf("Usage: %s <hostname>\n", argv[0]);
		return 1;
	}

	/* Lookup hostname */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(argv[1], "8888", &hints, &serveraddr) != 0) {
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
	if (hollywood_socket(fd, &hlywd_socket) != 0) {
		printf("Unable to create Hollywood socket\n");
		close(fd);
		return 5;
	}

	/* Set the playout delay to 100ms */
	set_playout_delay(&hlywd_socket, 100);

	/* Send 100 messages */
	for (i = 0; i < 100; i++) {
		sprintf(buffer, "Msg %d..", i);
		if (i % 2) {
			msg_len = send_message_time(&hlywd_socket, buffer, 150, 0, i, i, 20, 150);
		} else {
			msg_len = send_message_time(&hlywd_socket, buffer, 140, 0, i, i, 20, 150);
		}
		printf("Sending message number %d (length: %d)..\n", i, msg_len);
		if (msg_len == -1) {
			printf("Unable to send message\n");
			free(buffer);
			close(fd);
			return 6;
		}
		/* Wait for 20ms before sending the next message */
		usleep(20000);
	}

	/* Free message buffer */
	free(buffer);

	/* Close socket */
	close(fd);

	return 0;
}
