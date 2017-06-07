//
//  playout_buffer.c
//  
//
//  Created by Saba Ahsan on 17/03/17.
//
//

#include "playout_buffer.h"
#define PLAYOUT_BUFFER "PLAYOUT_BUFFER"
//#define PLAYOUT_BUFFER ""

int is_empty (struct playout_buffer * q)
{
    if (q->qlen == 0)
    {
    //    printf("qeue is empty\n");fflush(stdout);

        return 1;
    }
    else
        return 0; 
}


int is_full (struct playout_buffer * q, int new_seq)
{
    if (q->qlen == MAX_QUEUED_MSGS && new_seq>q->highest_seqnum)
    {
    //    printf("qeue is full\n");fflush(stdout);
        return 1;
    }
    else
        return 0;
}


/*partial message can be popped as well*/
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
        ret = datalen;
    }
    else
    {
        ret = q->datalen[q->head];
    }
    
    q->datalen[q->head] -= ret;

    memcpy (buf, q->buf[q->head], ret);
    
    if(q->datalen[q->head]!=0)
    {
        memcpy(q->buf[q->head], q->buf[q->head]+ret, q->datalen[q->head]);
        printdebug(PLAYOUT_BUFFER, "Partial Pop seqnum %d, index %d (%d)\n", q->lowest_seqnum, q->head, ret);

    }
    else
    {
        memzero (q->buf[q->head], HOLLYWOOD_MSG_SIZE);
        q->qlen --;
        q->head = q->head + 1;
        if (q->head >= MAX_QUEUED_MSGS)
            q->head = q->head - MAX_QUEUED_MSGS;
        q->lowest_seqnum++;
        printdebug(PLAYOUT_BUFFER, "Pop seqnum %d, index %d (%d)\n", q->lowest_seqnum, q->head, ret);


    }
    
    return ret;
    
}

int push_message(struct playout_buffer * q, uint8_t * buf, uint32_t new_seq, uint32_t datalen)
{
    q->total_bytes_received+=datalen;
    if (datalen > HOLLYWOOD_MSG_SIZE)
    {
        printf("playout_buffer: Message to be queued is too long (%d bytes)\n", datalen);
        return -1;
    }
    
    int seq_gap = new_seq - q->lowest_seqnum;
    
    if ( seq_gap >= MAX_QUEUED_MSGS)
    {
        printf("playout_buffer: Irregular sequence number %u, too large (CURR head: %u)\n", new_seq, q->lowest_seqnum);
        return -1;
    }
    else if (seq_gap < 0)
    {
       // printf("playout_buffer: Irregular sequence number %u, expired (CURR head: %u)\n", new_seq, q->lowest_seqnum);
        q->late_or_duplicate_packets++;
        return -1;
    }
    
    int curr_index = q->head + seq_gap;
    
    if (curr_index >= MAX_QUEUED_MSGS)
    {
        curr_index = curr_index - MAX_QUEUED_MSGS;
    }
    
    if (q->highest_seqnum < new_seq )
    {
        q->highest_seqnum = new_seq;
        q->qlen = seq_gap + 1;
    }
    else /*check for duplicate*/
    {
        if (q->datalen[curr_index] != 0)
        {
            if(q->datalen[curr_index] == datalen)
                printdebug(PLAYOUT_BUFFER, "Duplicate message seq : %u (Len new: %d, old: %d)\n", new_seq);
            else
                printdebug(PLAYOUT_BUFFER, "ERROR: Mismatched duplicate message seq : %u (Len new: %d, old: %d)\n", new_seq, q->datalen[curr_index], datalen);
            return -1;
        }
    }
    
    if (q->qlen == 0)
    {
        q->qlen = seq_gap + 1;
    }
    
    memcpy(q->buf[curr_index], buf, datalen);
    if( new_seq > q->highest_seqnum)
    {
        q->highest_seqnum = new_seq;
    }
    
    q->datalen[curr_index] = datalen;
    
    printdebug(PLAYOUT_BUFFER, "Push seqnum %d (%d-%d, qlen %d), index %d(%d) (size: %d)\n", new_seq, q->highest_seqnum, q->lowest_seqnum, q->qlen, curr_index, q->head, datalen);
    
    q->total_bytes_pushed+=datalen;

    return new_seq;
}

