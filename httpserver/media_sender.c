/*
 * Copyright (c)
 *      2012 Stefano Sabatini (FFmpeg example portions)
 *      2017 Saba Ahsan
 *      2017 Stephen McQuistin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "media_sender.h"
#include "tsdemux.h"

#include <sys/stat.h>

pthread_t       av_tid;         /*thread id of the av parser thread*/
pthread_t       h_tid;          /*thread id of the tcp hollywood sender thread*/
pthread_cond_t  msg_ready;      /*indicates whether a full message is ready to be sent*/
pthread_mutex_t msg_mutex;      /*mutex of the hlywd_message*/

extern uint64_t offset;         /*offset added to last 4 bytes of the message*/
extern uint32_t stream_seq;
extern int ContentLength;
static int BytesSent = 0;
#define MEDIA_SENDER "MEDIA_SENDER"

vid_frame *video_frames = NULL;

/* Add message to the queue of messages*/
int add_msg_to_queue(struct hlywd_message *msg, struct parse_attr *pparams) {
    
    /* update offset to include this message */    
    offset+=msg->msg_size;
    /* copy offset to end of message */
    uint64toa(msg->message+msg->msg_size, offset); 
    
    /* copy stream sequence number to end of message */
    uint32toa(msg->message+msg->msg_size+sizeof(offset), stream_seq); 
    
    /* update message size to include offset, stream sequence number */
    msg->msg_size+=HLYWD_MSG_TRAILER;

    /* increment stream sequence number */
    stream_seq++;
    
    pthread_mutex_lock(&msg_mutex);
    
    /* if MAXQLEN is reached, wait for it to empty*/
    while (pparams->hlywd_data->qlen>=MAXQLEN) {
        pthread_cond_wait(&msg_ready, &msg_mutex);
    }
    
    /* if the queue is empty, then this is the first entry */
    if (pparams->hlywd_data->hlywd_msg == NULL)
    {
        pparams->hlywd_data->hlywd_msg = msg;
    } else {
        /* otherwise, add this message to the tail of the queue */
        int qlen = 1;
        struct hlywd_message *tmp = pparams->hlywd_data->hlywd_msg;
        while (tmp->next != NULL) {
            tmp = tmp->next;
            qlen++;
        }
        tmp->next = msg;
    }
    
    pparams->hlywd_data->qlen++;
    
    /* if the queue was empty, send signal to wake up any sleeping send thread */
    pthread_cond_signal(&msg_ready);
    pthread_mutex_unlock(&msg_mutex);
    return 1; 
}

/* Fill in Hollywood timing metadata */
void *fill_timing_info(void *pparams_arg) {
    struct parse_attr *pparams = (struct parse_attr *) pparams_arg;
    struct hlywd_message *msg;
    int bytes_read;
    int message_size = HOLLYWOOD_MSG_SIZE - HLYWD_MSG_TRAILER;

    /* 
     * for each frame:
     *  while not all frame bytes read/sent:
     *       read min(remaining bytes, message_size)
     *       queue message for sending
     */
    struct vid_frame *cur_frame = video_frames;
    while (cur_frame != NULL) {
        int64_t remaining_bytes = cur_frame->len;
        while (remaining_bytes > 0) {
            /* initialise an empty Hollywood message */
            msg = (struct hlywd_message *) malloc(sizeof(struct hlywd_message));
            memset(msg, 0, sizeof(struct hlywd_message));
            int64_t to_read = remaining_bytes;
            if (to_read > message_size) {
                to_read = message_size;
            }
            
            bytes_read = fread(msg->message, sizeof(unsigned char), to_read, pparams->fptr);
            
            if (bytes_read != to_read && !feof(pparams->fptr)) {
                /* fewer bytes were read, and EOF not reached: error */
                perror("An error occured while reading the file.\n");
                free(msg);
                return NULL;
            }
            
            /* set Hollywood message metadata */
            msg->msg_size = bytes_read;
            
            /* queue message */
            add_msg_to_queue(msg, pparams);
        
            /* lose pointer; free'd by sender */
            msg = NULL;
            
            remaining_bytes = remaining_bytes - bytes_read;
        }
        vid_frame *next = cur_frame->next;
        free(cur_frame);
        cur_frame = next;
    }
    
    pthread_mutex_lock(&msg_mutex);
    /* indicate that file has been completely read */
    pparams->hlywd_data->file_complete = 1;
    /* signal message ready to send */
    pthread_cond_signal(&msg_ready);
    pthread_mutex_unlock(&msg_mutex);
    
    return NULL;
}

/* Write message using Hollywood API */
void *write_to_hollywood(void *hlywd_data_arg) {
    struct hlywd_attr *hlywd_data = (struct hlywd_attr *) hlywd_data_arg;
    struct hlywd_message *msg;
    uint16_t depends_on;
    int msg_len;

    /* if not sending, open file to save to instead */
#ifdef NOSEND
    char filename[256] = "saved_output2.mp4";
    FILE *fptr;
    printf("Saving to file: %s\n", filename);
    fflush(stdout);
    fptr = fopen(filename,"wb");
    if (fptr == NULL) {
        perror("Error opening file:");
        return NULL;
    }
#endif
    
    while(1) {
        pthread_mutex_lock(&msg_mutex);
        
        /* if queue is empty and EOF not reached, wait for more messages */
        while (hlywd_data->qlen == 0 && !hlywd_data->file_complete)
        {
            pthread_cond_wait(&msg_ready, &msg_mutex);
        }
        
        /* if queue is empty and EOF has been reached, stop */
        if (hlywd_data->qlen == 0 && hlywd_data->file_complete) {
            pthread_mutex_unlock(&msg_mutex);
            break;
        }
        
        /* get message at top of queue */
        msg = hlywd_data->hlywd_msg;
        
        /* check message isn't empty */
        if (msg == NULL) {
            printf("Error: Empty message was found in the sender queue\n");
            break;
        }
        
        /* dequeue message at top of queue */
        hlywd_data->hlywd_msg = hlywd_data->hlywd_msg->next;
        hlywd_data->qlen--;
        
        /* if the queue was full, send a signal to wake parser thread */
        if (hlywd_data->qlen <= MAXQLEN) {
            pthread_cond_signal(&msg_ready);
        }
        
        /* if not sending, write message to file */
#ifdef NOSEND
        if (fwrite(msg->message,sizeof(unsigned char), msg->msg_size, fptr)!=msg->msg_size) {
            if (ferror(fptr)) {
                printf("Error Writing to %s\n", filename);
            }
            perror("The following error occured\n");
            free(msg);
            pthread_mutex_unlock(&msg_mutex);
            break;
        }     
#else
        /* otherwise, use Hollywood API to send the message */
        if (msg->depends_on != 0) {
            depends_on = hlywd_data->seq + msg->depends_on;
        } else {
            depends_on = 0;
        }
        msg_len = send_message_time(hlywd_data->hlywd_socket, msg->message, msg->msg_size, 0, hlywd_data->seq, depends_on, msg->lifetime_ms);
//	BytesSent+=msg->msg_size-8; 
//        uint32_t tmp; 
//	memcpy(&tmp, msg->message + msg->msg_size - sizeof(uint32_t), sizeof(uint32_t));
//        tmp = ntohl(tmp); 
//        printdebug(MEDIA_SENDER, "Sending %d of %d (%d) seq: %u\n", BytesSent, ContentLength, msg_len, tmp );
        hlywd_data->seq++;
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
    return NULL;  
}

/* Send media file using Hollywood */
int send_media_over_hollywood(hlywd_sock * sock, FILE *fptr, int seq, char *src_filename) {
    struct hlywd_attr hlywd_data = {0};
    struct parse_attr pparams = {0};
    pthread_attr_t attr;

    /* initialise attribute structures */
    pparams.fptr = fptr; /*file pointer for reading mp4 file*/
    pparams.hlywd_data = &hlywd_data; /* Hollywood metadata structure */
    pparams.src_filename = src_filename;
    pparams.streams = NULL;
    hlywd_data.hlywd_socket = sock; /* Hollywood socket */
    
    /* if sending, set playout delay via Hollywood API */
#ifndef NOSEND 
    set_playout_delay(hlywd_data.hlywd_socket, 100); /* Set the playout delay to 100ms */
#endif
    
    /* Set sequence number */
    hlywd_data.seq = seq;

    char *file_ext = strrchr(src_filename, '.') + 1;
        
    struct stat src_file_stat;
    stat(src_filename, &src_file_stat);
    size_t src_filesize = src_file_stat.st_size;

    if(strcmp(file_ext, "ts") == 0) {
        video_frames = get_frames(&pparams);
    } else {
        struct vid_frame *new_frame = (struct vid_frame *) malloc(sizeof(struct vid_frame));
        new_frame->starts_at = 0;
        new_frame->next = NULL;
        new_frame->timestamp = 0;
        new_frame->key_frame = 0;
        new_frame->len = src_filesize;
        video_frames = new_frame; 
    }
    
    BytesSent = 0; 
    /* Initialize the condition and mutex */
    pthread_cond_init(&msg_ready, NULL);
    pthread_mutex_init(&msg_mutex, NULL);
    
    /* Initialize the threads, creating joinable for portability ref: 
     *    https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables
     */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    /* Create parser thread, passing pparams as argument */
    int err = pthread_create(&(av_tid), &attr, &fill_timing_info, (void *) &pparams);
    
    if (err != 0) {
        printf("\ncan't create parser thread :[%s]", strerror(err));
    }
    
    /* Create sender thread, passing hlywd_data as argument */
    err = pthread_create(&(h_tid), &attr, &write_to_hollywood, (void *) &hlywd_data);
    
    if (err != 0) {
        printf("\ncan't create hollywood sender thread :[%s]", strerror(err));
    }
    
    /* Wait for threads to end */
    pthread_join(av_tid, NULL);
    pthread_join(h_tid, NULL);
    
    seq = hlywd_data.seq;
    offset = 0;
    
    /* Destroy the attr, mutex & condition */
    pthread_attr_destroy(&attr);
    pthread_cond_destroy (&msg_ready);
    pthread_mutex_destroy(&msg_mutex);
    
    return seq;
}
