//
//  media-sender.c
//  
//
//  Created by Saba Ahsan on 17/02/17.
//
//
#include "media_sender_timeless.h"



pthread_t       av_tid;          /*thread id of the av parser thread*/
pthread_t       h_tid;          /*thread id of the tcp hollywood sender thread*/
pthread_cond_t  msg_ready;      /*indicates whether a full message is ready to be sent*/
pthread_mutex_t msg_mutex;      /*mutex of the hlywd_message*/

extern uint64_t offset;     /*offset added to last 4 bytes of the message*/
extern uint32_t stream_seq;


/* Add message to the queue of messages*/
int add_msg_to_queue ( struct hlywd_message * msg, struct parse_attr * p )
{

   /* update offset to include this message */    
    offset+=msg->msg_size;
    /* copy offset to end of message */
    uint64toa(msg->message+msg->msg_size, offset); 
    
    /* copy stream sequence number to end of message */
    uint32toa(msg->message+msg->msg_size+sizeof(uint64_t), stream_seq); 
    //printf("Lenght: %d OFFSET: %llu SEQ : %u\n", msg->msg_size, offset, stream_seq);
    
    /* update message size to include offset, stream sequence number */
    msg->msg_size+=HLYWD_MSG_TRAILER;

    
    ++stream_seq;
    
    pthread_mutex_lock(&msg_mutex);
    
    /* if MAXQLEN is reached, wait for it to empty*/
    while (p->h->qlen>=MAXQLEN)
        pthread_cond_wait(&msg_ready, &msg_mutex);
    
    if(p->h->hlywd_msg == NULL)
    {
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
        tmp->next = msg;
    }
    
    p->h->qlen++;
    
    /* if the queue was empty, send signal to wake up any sleeping send thread*/
    pthread_cond_signal(&msg_ready);
    pthread_mutex_unlock(&msg_mutex);
    return 1; 
    
}

void * fill_timing_info(void * a)
{
    /* TODO:  Fill timing info! */

    struct parse_attr *     p = (struct parse_attr *) a;
    struct hlywd_message *  msg;
    int                     bytes_read;
    int message_size = HOLLYWOOD_MSG_SIZE - HLYWD_MSG_TRAILER;
    
    while(!feof(p->fptr))
    {
        msg = (struct hlywd_message * ) malloc ( sizeof(struct hlywd_message) );

        memset ( msg, 0, sizeof(struct hlywd_message) );

         bytes_read = fread ( msg->message, sizeof(unsigned char), HOLLYWOOD_MSG_SIZE - HLYWD_MSG_TRAILER , p->fptr );
        
        if ( bytes_read != message_size )
        {
            if ( !feof(p->fptr) )
            {
                perror ("An error occured while reading the file.\n");
                free ( msg );
                return NULL;
                
            }
            else if (bytes_read == 0)
                break;
        }
        /* TODO : All TCP Hollywood fields are set to zero,
         check if this is the right behavior when we don't want partial reliability */

        msg->msg_size = bytes_read;

        add_msg_to_queue ( msg, p );
        msg=NULL;   /*once queued, lose the pointer. Memory is freed by sender.*/
        
    }
    
    pthread_mutex_lock(&msg_mutex);
    p->h->file_complete = 1;
    pthread_cond_signal(&msg_ready);
    pthread_mutex_unlock(&msg_mutex);
  //  printf("Ending read thread\n");

    return NULL;
}


void * write_to_hollywood(void * a)
{
    struct hlywd_attr * h       = (struct hlywd_attr *) a;
    struct hlywd_message * msg;
    uint16_t depends_on;
    int msg_len;

//    h->seq=                     0;
    
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
        
        /* if queue is empty wait for more messages*/
        while ( h->qlen == 0 && !h -> file_complete )
        {
            pthread_cond_wait( &msg_ready, &msg_mutex );
        }
        
    
        if( h->qlen == 0 && h->file_complete )
        {
          //  printf("Sending complete\n");
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
        if(h->qlen <= MAXQLEN)
            pthread_cond_signal(&msg_ready);
        h->qlen--;
        
#ifdef NOSEND
        if(fwrite (msg->message,sizeof(unsigned char), msg->msg_size, fptr)!=msg->msg_size)
        {
            if (ferror (fptr))
                printf ("Error Writing to %s\n", filename);
            perror("The following error occured\n");
            free(msg);
            pthread_mutex_unlock(&msg_mutex);
            break;
        }
        
#else
        if ( msg->depends_on!=0)
            depends_on = h->seq + msg->depends_on;
        else
            depends_on = 0;
        msg_len = send_message_time(h->hlywd_socket, msg->message, msg->msg_size, 0, h->seq, depends_on, msg->lifetime_ms);
        //printf("Sending message number %d (length: %d (size %d))..depends on : %u , lifetime : %u \n", h->seq, msg_len, msg->msg_size, depends_on, msg->lifetime_ms );
        fflush(stdout);
        h->seq++;
        if (msg_len == -1) {
            printf("Unable to send message over Hollywood\n");
            free(msg);
            pthread_mutex_unlock(&msg_mutex);
            break; 
        }
#endif
        free(msg);
        pthread_mutex_unlock(&msg_mutex);
        
    }
 //   printf("Ending write thread\n");
    return NULL;  
}



int send_media_over_hollywood(hlywd_sock * sock, FILE * fptr, int seq)
{
//    struct hlywd_attr   * h = (struct hlywd_attr * ) malloc (sizeof(struct hlywd_attr));
//    struct parse_attr   * p = (struct parse_attr * ) malloc (sizeof(struct parse_attr));
//    
//    memset(p, 0, sizeof(struct parse_attr));
//    memset(h, 0, sizeof(struct hlywd_attr));
    
    struct hlywd_attr h     = {0};
    struct parse_attr p     = {0};
    
    pthread_attr_t attr;
    p.fptr = fptr; /*file pointer for reading mp4 file*/
    p.h = &h;
    h.hlywd_socket = sock ;
#ifndef NOSEND
    
//    /*Initialize the hollywood socket*/
//    if (hollywood_socket(fd, &(h->hlywd_socket), 1, 0) != 0) {
//        printf("Unable to create Hollywood socket\n");
//        return 5;
//    }
//    
    set_playout_delay(h.hlywd_socket, 100); /* Set the playout delay to 100ms */
#endif
    
    h.seq = seq; /*Start the sequence numbers*/
    
    /*Open file*/
//    p->fptr=fopen(filename,"rb");
//    if (!p->fptr)
//    {
//        perror ("Error opening file:");
//        return 7;
//    }
    
    /*Initialize the condition and mutex*/
    pthread_cond_init(&msg_ready, NULL);
    pthread_mutex_init(&msg_mutex, NULL);
    
    /*Initialize the threads, creating joinable for portability ref: https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables*/
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    int err =pthread_create(&(av_tid), &attr, &fill_timing_info, (void *)&p);
    
    if(err!=0)
        printf("\ncan't create parser thread :[%s]", strerror(err));
    err =pthread_create(&(h_tid), &attr, &write_to_hollywood, (void *)&h);
    if(err!=0)
        printf("\ncan't create hollywood sender thread :[%s]", strerror(err));
    
    /*wait for threads to end*/
    pthread_join(av_tid, NULL);
    pthread_join(h_tid, NULL);
    
    /*close the file*/
//    if(p->fptr)
//    {
//        printf("Closing file\n"); fflush(stdout);
//        fclose(p->fptr);
//    }
    
    /*free the structures*/
   // free(h);
   // free(p);
    seq = h.seq;
    /*destroy the attr, mutex & condition*/
    pthread_attr_destroy(&attr);

    pthread_cond_destroy (&msg_ready);

    pthread_mutex_destroy(&msg_mutex);

    //pthread_exit(NULL);
    
    return seq;
}
