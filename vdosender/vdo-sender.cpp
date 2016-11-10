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

#include "vdo-sender.h"
//#define NOSEND

int lex_luther=0; 
int dropped_frames=0; 
bool endianness = false;
bool Mp4Model=true; 
metrics metric;
pthread_t p_tid; /*thread id of the parser thread*/
pthread_t h_tid; /*thread id of the tcp hollywood sender thread*/ 
pthread_cond_t msg_ready; /*indicates whether a full message is ready to be sent*/ 
pthread_mutex_t msg_mutex; /*mutex of the hlywd_message*/



int main(int argc, char *argv[]) 
{
   struct hlywd_attr * h = (struct hlywd_attr * )malloc (sizeof(struct hlywd_attr)); 
	struct parse_attr * p = (struct parse_attr *)malloc (sizeof(struct parse_attr));
	memzero(p, sizeof(struct parse_attr));
	memzero(h, sizeof(struct hlywd_attr));
	if(h->hlywd_msg==NULL)
		printf("Queue initialized to NULL\n"); fflush(stdout); 
	pthread_attr_t attr;
	int fd=-1; /*socket file descriptor*/
	p-> fptr = NULL; /*file pointer for reading mp4 file*/
	p->m= mp4_initialize(); /*initialize the mp4 file parser instance*/ 
	p->h = h; 
	
	/* Check for hostname parameter */
	if (argc != 3) {
		printf("Usage: %s <hostname> <file-to-send>\n", argv[0]);
		return 1; 
	}

	endianness = hostendianness();  	/*initialize endianness*/
	atexit(on_exit);  /*register exit function*/
#ifndef NOSEND 
	fd = initialize_socket(argv[1]); /*initialize socket*/
	if(fd<0)
		return 2; 


	/*Initialize the hollywood socket*/ 
	if (hollywood_socket(fd, &(h->hlywd_socket), 1) != 0) {
		printf("Unable to create Hollywood socket\n");
		return 5;
	}

	set_playout_delay(&(h->hlywd_socket), 100); /* Set the playout delay to 100ms */
#endif 
	h->seq = 0; /*Start the sequence numbers*/


	/*Initialize the file pointer*/ 
	/*Open file*/ 
	p->fptr=fopen(argv[2],"rb");
   if (!p->fptr)
	{
		perror ("Error opening file:");
		return 7;
	}

	/*Initialize the condition and mutex*/
	pthread_cond_init(&msg_ready, NULL); 
	pthread_mutex_init(&msg_mutex, NULL);

	/*Initialize the threads, creating joinable for portability ref: https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables*/
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	int err =pthread_create(&(p_tid), &attr, &parse_mp4file, (void *)p); 
	if(err!=0)
		printf("\ncan't create parser thread :[%s]", strerror(err));
	err =pthread_create(&(h_tid), &attr, &send_message, (void *)h); 
	if(err!=0)
		printf("\ncan't create hollywood sender thread :[%s]", strerror(err));
	

	/*wait for threads to end*/
	pthread_join(p_tid, NULL);
	printf("Parsing thread ended\n");
	pthread_join(h_tid, NULL);
	printf("Hollywood thread ended\n");

	END: 

	/*close the file*/
	if(p->fptr)
		fclose(p->fptr);

	/*close the socket*/
	if(fd>-1)
		close(fd); 

	/*free the structures*/
	mp4_destroy(&p->m); 
	free(h);
	free(p); 


	/*destroy the attr, mutex & condition*/
	pthread_attr_destroy(&attr);
	pthread_cond_destroy (&msg_ready); 
	pthread_mutex_destroy(&msg_mutex);
	pthread_exit(NULL);
}


/* Add message to the queue of messages*/
int add_msg_to_queue( struct hlywd_message * msg, struct parse_attr * p)
{
	pthread_mutex_lock(&msg_mutex);
	/* if MAXQLEN is reached, wait for it to empty*/
//	printf("\nParser: Queue length is %d \n", p->h->qlen);fflush(stdout);
//	printf("Queuing message of len: %d : %x\n", msg->msg_size, msg->message); fflush(stdout);  

	while (p->h->qlen>=MAXQLEN)
		pthread_cond_wait(&msg_ready, &msg_mutex);
	if(p->h->qlen==0 && p->h->hlywd_msg != NULL)
		printf("Yet again!!/n"); fflush(stdout); 
	if(p->h->hlywd_msg == NULL)
	{
	//	printf("Adding the first message to queue\n"); fflush(stdout); 		
		p->h->hlywd_msg = msg; 
	}
	else
	{
		int qlen=1; 
		struct hlywd_message * tmp = p->h->hlywd_msg; 
		while(tmp->next!=NULL)
		{
			tmp = tmp->next;
			qlen++;  

		}
//		printf("Adding the %dth message to queue\n", qlen); fflush(stdout); 
		tmp->next = msg; 
	}
	printf("Queued message of len: %d : %x\n", p->h->hlywd_msg->msg_size, p->h->hlywd_msg->message); fflush(stdout);  

	p->h->qlen++; 	
	/* if the queue was empty, send signal to wake up any sleeping send thread*/
	pthread_cond_signal(&msg_ready);
   pthread_mutex_unlock(&msg_mutex);
	return 1; 
	
}

int mp4_send_mdat(struct parse_attr * p )
{
	int bytes_read;
	int lastkey; 
	struct mp4_i * m = &p->m;
	unsigned int lastdur=m->decodetime;
	struct hlywd_message * msg;
	if (m->samplemap.size()==0)
	{
		for( map<uint32_t, struct tinfo>::iterator ii=m->tracks.begin(); ii!=m->tracks.end(); ii++)
		{
			if((*ii).second.stable== NULL)
			{
				cout<<"Received mdat and the sample m->table is still empty for one of the m->tracks "<<(*ii).first<<endl;
				return -1;
			}
			unsigned int i;
			for( i=0; i< (*ii).second.entries; i++)
			{

	  			lex_luther++; 
				cout<<lex_luther<<endl; 
				msg = (struct hlywd_message *) malloc(sizeof(struct hlywd_message));
				memzero(msg, sizeof(struct hlywd_message)); 
				msg->msg_size=(*ii).second.stable[i].size;
				msg->message = (unsigned char *) malloc(msg->msg_size);
				bytes_read = fread(msg->message, sizeof(char), msg->msg_size, p->fptr);
				if(bytes_read!=msg->msg_size)
				{
					printf("Error reading file in Mdat\n");
					return -1;  
				}
				if(lex_luther%500==0) 
				{

					memset(msg->message, 0, msg->msg_size); 
					dropped_frames++; 
				}
				msg->framing_ms = (double)(lastdur)*1000/(double)m->timescale;/*timestamp of the msg*/
				msg->lifetime_ms = (double)((*ii).second.stable[i].sdur)*1000/(double)m->timescale;
				if((*ii).second.stable[i].key>0)
				{
					msg->depends_on = 0; /*value will be added to own seq # at the time of queueing*/
					lastkey = round((double)((*ii).second.stable[i].key)/(double)((*ii).second.stable[i].sdur)); 
				}
				else 
				{
					msg->depends_on = -(lastkey); 
				}
				lastdur += (*ii).second.stable[i].sdur;
				/*Send the message*/
				printf("Sending message of len: %d : %x\n", msg->msg_size, msg->message); fflush(stdout); 
//				cout<<"youtubeevent13\t"<<(*ii).second.stable[i].stime<<"\t"<<(*ii).second.stable[i].size<<"\t"<< (*ii).second.stable[i].sdur<<"\t"<<(*ii).second.stable[i].key<<"\t"<<msg->framing_ms<<"\t"<<msg->lifetime_ms<<"\t"<<msg->depends_on<<endl;
				add_msg_to_queue(msg, p); 
				msg=NULL; /*once queued, lose the pointer. Memory is freed by sender.*/
				lastkey--; 

			}
			delete [] (*ii).second.stable;
			(*ii).second.stable = NULL;
		}

	}
	printf("FINISHED MDAT\n"); fflush(stdout); 
	return 1; 
}

void * parse_mp4file(void * a)
{
	int bytes_read;  
	int leftoverbytes=0;
	int readlen, headerlen; 
	unsigned char header[HEADERLEN];
	struct parse_attr * p = (struct parse_attr *) a; 
	struct hlywd_message * msg;

	while(1)
	{
		msg = (struct hlywd_message *) malloc(sizeof(struct hlywd_message));
		memzero(msg, sizeof(struct hlywd_message)); 
		/*read the first header from the file*/ 
		readlen = HEADERLEN;
		bytes_read = fread(header, sizeof(char), readlen, p->fptr);
		if(bytes_read!=readlen)
		{
			if (!feof(p->fptr))
				goto FILEERROR;
			else if(bytes_read==0)
				break;  
		}
		fseek (p->fptr, -(readlen), SEEK_CUR); /* rewind the file after reading header*/ 
		/*Send the mp4 parser for extracting the size and other params of the message*/                
		headerlen = get_msg_size(header, bytes_read, msg);fflush(stdout);
		if(msg->msg_size<=0)
		{
			printf("Error: Mp4 block message size could not be read\n");
			break;
		}
		if(header[4]=='m' && header[5]=='d' && header[6]=='a' && header[7]=='t')
		{
			/*if mdat we will just send the header now and send the frames later. */
//			printf("Curr postions: %d, MDAT: %d, New position: %d\n", ftell( p->fptr), msg->msg_size, ftell( p->fptr)+msg->msg_size); 
			msg->msg_size = headerlen; 
			msg->message = (unsigned char *) malloc(msg->msg_size);
			readlen = msg->msg_size;
			bytes_read = fread(msg->message, sizeof(char), readlen, p->fptr);
			if(bytes_read!=readlen)
				goto FILEERROR; ; 
//			printf("Sending message of len: %d : %x\n", msg->msg_size, msg->message); fflush(stdout);  
			add_msg_to_queue(msg, p); 
			msg=NULL; /*once queued, lose the pointer. Memory is freed by sender.*/
			/*Now send the frames*/
			if (mp4_send_mdat(p)<0)
				break; 
//			printf("Returning to sending metadata\n");  

		}
		else 
		{
			/*It's just metadata, read the full message and send*/ 
			msg->message = (unsigned char *) malloc(msg->msg_size);
			readlen = msg->msg_size;
			bytes_read = fread(msg->message, sizeof(char), readlen, p->fptr);
			if(bytes_read!=readlen)
				break; 
			if(mp4_savetag (msg->message, msg->msg_size, &p->m)<0)
			{
				printf("Error: Mp4 parsing failure\n");
				break;
			}
			msg->lifetime_ms = 10000; /*default lifetime of 10 seconds for metadata*/ 
			msg->depends_on = 0; /* only on itself*/
			/*Send the message*/
			add_msg_to_queue(msg, p); 
			msg=NULL; /*once queued, lose the pointer. Memory is freed by sender.*/
		}
		
	}

	pthread_mutex_lock(&msg_mutex);
	p->h->file_complete = true; 
	pthread_cond_signal(&msg_ready);
   pthread_mutex_unlock(&msg_mutex); 
	printf("Dropped frames %d \n", dropped_frames); 
	return NULL; 

	FILEERROR:
	if (feof(p->fptr)) 
	{
		printf ("Error:End-of-File reached prematurely, last bytes read %d.\n", bytes_read);
	}
	else 
	{
		perror ("An error occured while reading the file.\n");
	}
	free(msg->message); 
	free(msg);
	return NULL;
}



void * send_message(void * a)
{
	struct hlywd_attr * h = (struct hlywd_attr *) a; 
	struct hlywd_message * msg;
	h->seq=0; 
	int msg_len; 

#ifdef NOSEND
	char filename[256]="saved_output2.mp4";
	FILE *fptr;
	printf("Saving to file: %s\n", filename); fflush(stdout);
   fptr=fopen(filename,"wb");
	if (fptr==NULL)
	{
		perror ("Error opening file:");
		return NULL; 
	}
#endif

	while(1)
	{
		pthread_mutex_lock(&msg_mutex);
//		printf("\nSender: Queue length is %d \n", h->qlen);fflush(stdout);

			
		/* if queue is empty wait for more messages*/
		while (h->qlen==0 && !h->file_complete)
			pthread_cond_wait(&msg_ready, &msg_mutex);
		if(h->qlen==0 && h->file_complete)
		{
			printf("Sending complete\n"); 
			pthread_mutex_unlock(&msg_mutex);
			break; 
		}
		msg = h->hlywd_msg; 
		if(msg==NULL)
		{
			printf("Error: Empty message was found in the sender queue\n");
			break;
		}

		h->hlywd_msg = h->hlywd_msg->next; 
		/*if the queue was full, send a signal to wake parser thread*/
		if(h->qlen<=MAXQLEN)
			pthread_cond_signal(&msg_ready);
		h->qlen--; 	

#ifdef NOSEND
		printf("Sender: Writing to file\n"); fflush(stdout); 
		if(fwrite (msg->message,sizeof(unsigned char), msg->msg_size, fptr)!=msg->msg_size)
		{
			if (ferror (fptr))
				printf ("Error Writing to %s\n", filename); 
			perror("The following error occured\n");
			free(msg->message); 
			free(msg);
			pthread_mutex_unlock(&msg_mutex);
			break;  	
		}

#else
		msg_len = send_message_time(&h->hlywd_socket, msg->message, msg->msg_size, 0, h->seq, msg->depends_on, msg->lifetime_ms);
		printf("Sending message number %d (length: %d)..\n", h->seq, msg_len);
		h->seq++; 
		if (msg_len == -1) {
			printf("Unable to send message\n");
			free(msg->message); 
			free(msg);
			pthread_mutex_unlock(&msg_mutex);
			break; 
		}
#endif
		printf("Sender: Unlocking mutex \n"); fflush(stdout); 
		free(msg->message); 
		free(msg); 
		pthread_mutex_unlock(&msg_mutex);

	}
#ifdef NOSEND
	fclose(fptr);
#endif  
	return NULL;  
}

/* returns 0 on success, else the error code */ 
int initialize_socket(const char * hostname)
{
	int fd;
	struct addrinfo hints, *serveraddr; 
   /* Lookup hostname */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(hostname, "8882", &hints, &serveraddr) != 0) {
		printf("Hostname lookup failed\n");
		return -1; 
	}

	/* Create a socket */
	if ((fd = socket(serveraddr->ai_family, serveraddr->ai_socktype, serveraddr->ai_protocol)) == -1) {
		printf("Unable to create socket\n");
		return -1;
	}

	/* Connect to the receiver */
	if (connect(fd, serveraddr->ai_addr, serveraddr->ai_addrlen) != 0) {
		printf("Unable to connect to receiver\n");
		return -1;
	}
	return fd; 
}

void on_exit()
{
	
}




