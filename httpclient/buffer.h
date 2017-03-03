//
//  buffer.h
//  
//
//  Created by Saba Ahsan on 17/01/17.
//
//

#ifndef ____buffer__
#define ____buffer__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "helper.h"

#define MAX_OUTOFORDER_MSGS 10

/*Buffer size of 50MB, would hold for instance 50 seconds of a 8Mbps video*/
#define MAX_BUFFER_SIZE 50*1024*1024



struct mm_buffer {
    uint8_t buf[MAX_BUFFER_SIZE];
    int start;
    int end;
    uint32_t first_offset; /* offset of the next message (last_parsed_seqnum+1) in the stream */
    uint32_t last_parsed_seqnum;
    uint32_t highest_saved_seqnum; 
    int saved_seqnum[MAX_OUTOFORDER_MSGS];
    uint32_t saved_readlen[MAX_OUTOFORDER_MSGS];
    int first_byte[MAX_OUTOFORDER_MSGS];
};


int save_new_msg(uint32_t curr_seq_num, char * msg, int readlen, int bufsize, uint32_t curr_offset );
int get_buf_freespace ( );
int pop_contiguous_msg(char * msg, int bufsize, int force);
int new_packet( uint32_t curr_seq_num, char * msg, int readlen, int bufsize, uint32_t curr_offset );
int is_msg_queued(uint32_t seq_num);
int init_mmbuffer();
void shift_all_indices(int index);


#endif /* defined(____buffer__) */
