//
//  buffer.c
//  
//
//  Created by Saba Ahsan on 17/01/17.
//
//

#include "buffer.h"

struct mm_buffer * m;

int init_mmbuffer()
{
    m = malloc(sizeof(struct mm_buffer));
    if(m == NULL)
        return -1;
    memzero ( m , sizeof(struct mm_buffer));
    for( int i =0; i< MAX_OUTOFORDER_MSGS; i++)
        m->saved_seqnum[i] = -1;
    return 0;
}

void shift_all_indices(int index)
{
    for ( int i =0; i< MAX_OUTOFORDER_MSGS-index-1; i++)
    {
        m->saved_readlen[i] = m->saved_readlen[i+index+1];
        m->first_byte[i] = m->first_byte[i+index+1];
        m->saved_seqnum[i] = m->saved_seqnum[i+index+1];
    }
    for ( int i = MAX_OUTOFORDER_MSGS-index-1; i < MAX_OUTOFORDER_MSGS; i++)
    {
        m->saved_readlen[i] = 0;
        m->first_byte[i] = 0;
        m->saved_seqnum[i] = -1;
    }
}


/*  return value :
 >0  : packet is valid, forward msg to the parser
 -1  : packet is old, just discard it.
  0  : packet was saved to queue
 */
int new_packet( uint32_t curr_seq_num, char * msg, int readlen, int bufsize, uint32_t curr_offset )
{
//    printf("\nNew packet: ");
//    for(int j =0; j<MAX_OUTOFORDER_MSGS; j++)
//        printf("%d ",m->saved_seqnum[j]);
//    printf("\n");
//    printf("\nNew packet: first_offset: %d, curr_offset: %d\n",m->first_offset, curr_offset );
//    printf("\nNew packet: curr_seq_num: %d, last_parsed_seqnum: %d\n", curr_seq_num, m->last_parsed_seqnum );
    
    if ((m->first_offset==0) && m->first_offset==curr_offset)
    {
        m->last_parsed_seqnum = curr_seq_num;
        m->first_offset = curr_offset + readlen;
    //    printf("1. New packet : New first offset %d \n", m->first_offset);

        return readlen;
    }
    
    if ((curr_seq_num == m->last_parsed_seqnum + 1) && m->first_offset==curr_offset)
    {
        m->last_parsed_seqnum = curr_seq_num;
        m->first_offset = curr_offset + readlen;
        /*if packets are queued, remove bytes reserved for this message*/
        if(m->start!=m->end)
        {
            shift_all_indices(0);
            m->start += readlen;
            if(m->start >= MAX_BUFFER_SIZE)
            {
              //  printf("Found first missing packet, readjusting m->start : %d - %d = ", m->start, MAX_BUFFER_SIZE);
                m->start -= MAX_BUFFER_SIZE;
             //   printf("%d\n", m->start);
            }
        }
      //  printf("2. New packet : New first offset %d \n", m->first_offset);

        return readlen;
    }
    
    if ( curr_seq_num < m->last_parsed_seqnum )
    {
        printf("Buffer Error: Message too old\n");
        return 0;
    }
    
    if ( curr_seq_num - m->last_parsed_seqnum >  MAX_OUTOFORDER_MSGS)
    {
        printf("Buffer Error: Message exceeds max outoforder capacity\n");
        return -1;
    }
    
    if ( curr_seq_num > m->last_parsed_seqnum  || (m->first_offset==0 && m->first_offset<curr_offset))
    {
        return save_new_msg(curr_seq_num, msg, readlen, bufsize, curr_offset);
    }
    printf("No conditions of new packet were met, WTH!\n");
    return -1 ;
}



int pop_contiguous_msg(char * msg, int bufsize, int force)
{
    printf("\nBuffer status: ");fflush(stdout);
    for(int j =0; j<MAX_OUTOFORDER_MSGS; j++)
        printf("%d ",m->saved_seqnum[j]);
    printf("\n");fflush(stdout);
    
    int readlen, start;
    if (m->first_offset == 0)
        return 0;
  //  printf("%d to %d\n",m->last_parsed_seqnum ,m->saved_seqnum[0]);fflush(stdout);
    if(m->saved_seqnum[0]!=-1 && (m->last_parsed_seqnum + 1) == m->saved_seqnum[0])
    {
        readlen = m->saved_readlen[0];
        if (readlen >= bufsize)
        {
            printf("Buffer Error: Message pop failed due to insufficient buffer size\n");
            return -1;
        }
   //     printf("buffer dimensions %d - %d (%d-%d)---", m->start, m->end, m->first_byte[0], readlen);
        m->last_parsed_seqnum++;
        m->start = m->first_byte[0]+readlen;
        if(m->start >= MAX_BUFFER_SIZE)
        {
           // printf("Readjusting m->start : %d - %d = ", m->start, MAX_BUFFER_SIZE);
            m->start -= MAX_BUFFER_SIZE;
      //      printf("%d\n", m->start);
            memcpy(msg, m->buf+(m->first_byte[0]), MAX_BUFFER_SIZE-m->first_byte[0]);
            memcpy(msg+(MAX_BUFFER_SIZE-m->first_byte[0]), m->buf, readlen-(MAX_BUFFER_SIZE-m->first_byte[0]));
            memzero((m->buf)+(m->first_byte[0]), MAX_BUFFER_SIZE-m->first_byte[0]);
            memzero(m->buf, readlen-(MAX_BUFFER_SIZE-m->first_byte[0]));
        }
        else
        {
            memcpy(msg, m->buf+(m->first_byte[0]), readlen);
            memzero((unsigned char *)(m->buf)+(m->first_byte[0]), readlen);
        }
        shift_all_indices(0);
        m->first_offset += readlen;
       // printf("Pop packet : New first offset %d \n", m->first_offset);
        return readlen;
        
    }
    else if(force==1 || m->highest_saved_seqnum - m->last_parsed_seqnum == MAX_OUTOFORDER_MSGS)
    {
      //  printf("Force popping one message..."); fflush(stdout);
        /*Pop the first available message*/
        for(int i =0; i<MAX_OUTOFORDER_MSGS; i++)
        {
            if(m->first_byte[i]>0)
            {
              //  printf("Index %d...", i); fflush(stdout);
                m->last_parsed_seqnum = m->saved_seqnum[i];
                if(m->first_byte[i]>m->start)
                    readlen = (m->first_byte[i]-m->start) + m->saved_readlen[i];
                else
                    readlen = (m->first_byte[i]+(MAX_BUFFER_SIZE - m->start)) + m->saved_readlen[i];
                if (readlen >= bufsize)
                {
                    printf("Buffer Error: Message pop failed due to insufficient buffer size\n");
                    return -1;
                }

                start = m->start;
                m->start = m->first_byte[i] + m->saved_readlen[i];
                if(m->start >= MAX_BUFFER_SIZE)
                {
                    m->start -= MAX_BUFFER_SIZE;
                    memcpy(msg, m->buf+start, MAX_BUFFER_SIZE-start);
                    memcpy(msg+(MAX_BUFFER_SIZE-start), m->buf, readlen-(MAX_BUFFER_SIZE-start));
                    memzero((m->buf)+start, MAX_BUFFER_SIZE-start);
                    memzero(m->buf, readlen-(MAX_BUFFER_SIZE-start));
 
                }
                else
                {
                    memcpy(msg, m->buf+(start), readlen);
                    memzero((unsigned char *)(m->buf)+start, readlen);
                }
                shift_all_indices(i);
                m->first_offset += readlen;
             //   printf("Done"); fflush(stdout);
                return readlen;
            }
        }
    }

    return 0;
}


int save_new_msg(uint32_t curr_seq_num, char * msg, int readlen, int bufsize, uint32_t curr_offset )
{
    int relative_seq, first_byte;
    
    if (m->first_offset!=0)
        relative_seq = curr_seq_num - m->last_parsed_seqnum - 1;
    else
        relative_seq = curr_seq_num - 1;

//    printf("Saving: Message # %d of len %d (offset: %d) at location %d.... Buffer range %d - %d\n", curr_seq_num, readlen, curr_offset, relative_seq, m->start, m->end);
//    fflush(stdout);
    
    if(m->saved_seqnum[relative_seq] == curr_seq_num)
    {
        printf("Buffer Error: received duplicate of already saved message\n");
        return -1;
    }
    else if(m->saved_seqnum[relative_seq] != -1)
    {
        printf("Buffer Error: error in saving, possible logic error\n");
        printf("Relative seqnum saved %d\n", m->saved_seqnum[relative_seq]);
        return -1;
    }
    
    if((curr_offset - m->first_offset)+ readlen >= MAX_BUFFER_SIZE)
    {
        printf("Buffer Error: Message too big to be saved %d - %d\n",curr_offset, m->first_offset);
        return -1;
    }

    first_byte = m->start + (curr_offset - m->first_offset);
//    printf("%d = m->start %d + (curr_offset %d- m->first_offset%d\n",first_byte, m->start, curr_offset, m->first_offset);

    if (first_byte >= MAX_BUFFER_SIZE)
        first_byte -= MAX_BUFFER_SIZE;
//    printf("Saving: highest_saved_seqnum = %d, curr_seq_num = %d, first_byte = %d\n", m->highest_saved_seqnum, curr_seq_num, first_byte);
    if(m->highest_saved_seqnum < curr_seq_num)
    {
        m->highest_saved_seqnum = curr_seq_num;
        m->end = first_byte + readlen;
        if(m->end>=MAX_BUFFER_SIZE)
            m->end -= MAX_BUFFER_SIZE;
    }
    
    if((first_byte+readlen) >= MAX_BUFFER_SIZE)
    {
 //       printf("Copying w wraparound, range %d - %d...",m->start, m->end);fflush(stdout);
        memcpy(m->buf+first_byte, msg, MAX_BUFFER_SIZE-first_byte);
        memcpy(m->buf, msg+(MAX_BUFFER_SIZE-first_byte), readlen-(MAX_BUFFER_SIZE-first_byte));
    }
    else
    {
//        printf("Copying w/o wraparound, range %d - %d...",m->start, m->end);fflush(stdout);
        memcpy(m->buf+first_byte, msg, readlen);
    }
 //   printf("Done\n");fflush(stdout);
    m->saved_readlen[relative_seq] = readlen;
    m->first_byte[relative_seq] = first_byte;
    m->saved_seqnum[relative_seq] = curr_seq_num;

    return 0;
    
}
