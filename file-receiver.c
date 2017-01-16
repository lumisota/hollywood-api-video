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

#define BUFLEN 500000

int main(int argc, char *argv[]) {
	hlywd_sock h_sock;
	int fd, cfd;
	socklen_t cfd_len;
	struct sockaddr_in server_addr, client_addr;
	char buffer[BUFLEN];
	char filename[256]="saved_output";
	ssize_t read_len;
	uint8_t substream_id;
	FILE *fptr;

	printf("Usage: %s <file-to-save-to>\n", argv[0]);

	/* Check for filename parameter */
	if (argc > 1) {
		strcpy(filename, argv[1]); 
	}
	printf("Saving to file: %s\n", filename); fflush(stdout);
   fptr=fopen(filename,"wb");
	if (fptr==NULL)
	{
		perror ("Error opening file:");
		return 6; 
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
	if (hollywood_socket(cfd, &h_sock, 1, 0) != 0) {
		printf("Unable to create Hollywood socket\n");
		return 2;
	}

	/* Receive message loop */
	while ((read_len = recv_message(&h_sock, buffer, BUFLEN, 0, &substream_id)) > 0) {
		printf("Message received (substream %u) of len %zd:%d\n", substream_id, read_len, h_sock.current_sequence_num);
		if(fwrite (buffer , sizeof(char), read_len, fptr)!=read_len)
		{
			if (ferror (fptr))
				printf ("Error Writing to %s\n", filename); 
			perror("The following error occured\n");
			return 7;  	
		}
	}

	/* Close connection */
	close(cfd);
	close(fd);
	fclose(fptr); 

	return 0;
}
