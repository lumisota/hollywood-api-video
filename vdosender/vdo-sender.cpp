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

#include "mpeg.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define BUFLEN 200
bool endianness = false;
bool Mp4Model=true; 
metrics metric;

int main(int argc, char *argv[]) {
	int fd, seq, msg_len;
	size_t bytes_read;
	unsigned char *buffer = (unsigned char *) malloc(BUFLEN);
	int counter;
	FILE *fptr;
	int leftoverbytes=0;
	struct mp4_i m= mp4_initialize();

	/* Check for hostname parameter */
	if (argc != 3) {
		printf("Usage: %s <hostname> <file-to-send>\n", argv[0]);
		return 1;
	}

	/*initialize endianness*/
	endianness = hostendianness();

	/*Open file*/ 
	fptr=fopen(argv[2],"rb");
	if (!fptr)
	{
		perror ("Error opening file:");
		return 7;
	}

	/* Send the file contents */
	bytes_read = fread(buffer, 1,BUFLEN,fptr);
	while(bytes_read==BUFLEN) {

		printf("Sending message number %d (length: %d)..\n", seq, bytes_read);
		if (msg_len == -1) {
			printf("Unable to send message\n");
			free(buffer);
			fclose(fptr); 
			return 6;
		}
		int index=0;
		while(1)
		{
			if (bytes_read>(index+leftoverbytes))
				index+=leftoverbytes;
			else /*we don't have enough data to go to next header, update the leftoverbytes and break*/
			{
				leftoverbytes-=bytes_read-index;
				break;
			}
			/* save the flv/mp4/webm tag, the flv_savetag function returns the bodylength+11 of the tag read
			 * or if the tag is not fully received it returns the number of bytes that were saved
			 * the function also stores partial headers and takes care of resuming when more data
			 * is received so we don't need to worry about that here.mkv_savetag does same for mkv
			 */
			leftoverbytes=mp4_savetag((unsigned char *)buffer+index, bytes_read-index, &(m));
			if(leftoverbytes<0)
			{
				printf("Error in Download, exiting now");
				free(buffer);
				fclose(fptr); 
				return 8;
			}

		}

		/* Wait for 20ms before sending the next message */
		usleep(20000);
		bytes_read = fread(buffer,1,BUFLEN,fptr);
		seq++; 
	}

	
	if (feof(fptr)) {
		if(bytes_read > 0)
		{
			printf("Sending message number %d (length: %d). \n", seq, bytes_read);
		}
		printf ("End-of-File reached.\n");
	}
	else 
		printf ("An error occured while reading the file.\n");

	/* Free message buffer */
	free(buffer);

	/*close the file*/
	fclose(fptr);

	return 0;
}


