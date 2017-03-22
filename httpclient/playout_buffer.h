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
#include "helper.h"


/* Buffer size of ~15MB, would hold for instance ~15 seconds of 8Mbps video
 * and 1000 Hollywood messages, each of HOLLYWOOD_MSG_SIZE bytes*/
#define MAX_QUEUED_MSGS 10000


struct playout_buffer
{
    uint8_t buf[MAX_QUEUED_MSGS][HOLLYWOOD_MSG_SIZE];
    uint32_t datalen[MAX_QUEUED_MSGS];
    int head;
    int qlen;
    uint32_t lowest_seqnum;
    uint32_t highest_seqnum;
};

int pop_message (struct playout_buffer * q, uint8_t * buf, uint32_t datalen);

int push_message(struct playout_buffer * q, uint8_t * buf, uint32_t new_seq, uint32_t datalen);

int is_empty (struct playout_buffer * q);

int is_full (struct playout_buffer * q, int new_seq);


#endif /* defined(____playout_buffer__) */
