//
//  playout_buffer.h
//  
//
//  Created by Saba Ahsan on 17/03/17.
//
//

#ifndef ____playout_buffer__
#define ____playout_buffer__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../common/http_ops.h"


/* Buffer size of ~15MB, would hold for instance ~15 seconds of 8Mbps video
 * and 10240 Hollywood messages, each of HOLLYWOOD_MSG_SIZE bytes*/
#define MAX_QUEUED_MSGS 10*1024


struct playout_buffer
{
    uint8_t buf[MAX_QUEUED_MSGS][HOLLYWOOD_MSG_SIZE];
    uint32_t datalen[MAX_QUEUED_MSGS];
    int head;
    int qlen;
    uint32_t lowest_seqnum;
    uint32_t highest_seqnum;
};


int pop_message (struct playout_buffer * q, uint8_t * buf, uint32_t datalen)
{
    int ret;
    if (q->qlen == 0)
    {
        printf("playout_buffer: No message to pop, queue empty\n");
        return -1;
    }
    
    if (datalen < q->datalen[q->head])
    {
        printf("playout_buffer: Buffer is not big enough for popped message (%d bytes)\n", datalen);
        return -1;
    }
    
    memcpy (buf, q->buf[q->head], q->datalen[q->head]);
    memset (q->buf[q->head], 0 , HOLLYWOOD_MSG_SIZE);
    ret = q->datalen[q->head];
    q->datalen[q->head] = 0;
    q->qlen --;
    q->head = q->head ++;
    if (q->head >= MAX_QUEUED_MSGS)
        q->head = q->head - MAX_QUEUED_MSGS;
    q->lowest_seqnum++;
            

    return ret;

}

int push_message(struct playout_buffer * q, uint8_t * buf, uint32_t new_seq, uint32_t datalen)
{
    if (q->qlen == MAX_QUEUED_MSGS)
    {
        printf("playout_buffer: No more space to push message\n");
        return -1;
    }
    
    if (datalen > HOLLYWOOD_MSG_SIZE)
    {
        printf("playout_buffer: Message to be queued is too long (%d bytes)\n", datalen);
        return -1;
    }
    
    int seq_gap = new_seq - q->lowest_seqnum;
    
    if ( seq_gap >= q->qlen)
    {
        printf("playout_buffer: Irregular sequence number %u, too large\n", new_seq);
        return -1;
    }
    else if (seq_gap < 0)
    {
        printf("playout_buffer: Irregular sequence number %u, expired\n", new_seq);
        return -1;
    }
    
    int curr_index = q->head + seq_gap;
    
    if (curr_index >= MAX_QUEUED_MSGS)
    {
        curr_index = curr_index - MAX_QUEUED_MSGS;
    }
    
    if (q->highest_seqnum < new_seq)
    {
        q->highest_seqnum = new_seq;
        q->qlen = seq_gap + 1;
    }
    
    memcpy(q->buf[curr_index], buf, datalen);
    if( seq > q->highest_seqnum)
    {
        q->highest_seqnum = seq;
        if(q->tail!=curr_index)
            printf("WTF!!");
    }
    
    q->datalen[curr_index] = datalen;
    
    return seq;
}





#endif /* defined(____playout_buffer__) */
