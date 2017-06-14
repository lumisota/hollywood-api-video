//
//  mm_download.c
//  
//
//  Created by Saba Ahsan on 22/02/17.
//
//

#include "mm_download.h"

pthread_t       av_tid;          /*thread id of the av parser thread*/
#define BUFFER_DURATION 6000 /*milliseconds*/
#define IS_DYNAMIC      1
#define ENCODING_DELAY  3000




#define DOWNLOAD "DOWNLOAD"
//#define DOWNLOAD ""

extern int endnow; 

int download_segments( manifest * m, transport * t , long long stime, long throughput)
{
    char buf[HTTPHEADERLEN];
    struct bola_state bola          = {0};
    uint8_t rx_buf[HOLLYWOOD_MSG_SIZE] = {0};
    int ret                         = 0;
    int bytes_rx                    = 0;
    int contentlen                  = 0;
    int curr_segment                = 0;
    int curr_bitrate_level          = 1;
    char * curr_url                 = NULL;
    int http_resp_len               = 0;
    uint32_t new_seq                = 0;
    long long download_start_time            = 0;
    void * sock;
    long buffered_duration          = 0;
    uint8_t substream               = 0;
    int segment_start               = 0;
    long long delay;
    uint32_t end_offset             = 0;
    uint32_t curr_offset            = 0;

    /*Initialize bola, isDynamic is set to 1 (Live)*/
    curr_bitrate_level = calculateInitialState(m, IS_DYNAMIC, &bola);
    saveThroughput(&bola, throughput);  /*bps*/

    if ( t->Hollywood)
    {
        sock = &(t->h_sock);
    }
    else
    {
        sock = &(t->sock);
    }
    printf("\n");
    while (curr_segment < m->num_of_segments )
    {
    
        if(curr_segment == 0)
        {
            segment_start = 0;
            buffered_duration = 0;
        }
        else
        {
            pthread_mutex_lock(&t->msg_mutex);
            if(t->parser_exited)
            {
                t->stream_complete = 1;
                pthread_mutex_unlock(&t->msg_mutex);
                goto END_DOWNLOAD;
            }
            if(t->init_segment_downloaded==0 && curr_segment > 1)
                pthread_cond_signal(&t->init_ready);
            segment_start = m->segment_dur * (curr_segment - m->init);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            fflush(stdout);
            pthread_mutex_unlock(&t->msg_mutex);
        }
        
        
        if((delay = buffered_duration - BUFFER_DURATION) > 0 )
        {
            /*Delay due to bufferLevel > bufferTarget is added to BOLA placeholder buffer*/
            printdebug(DOWNLOAD, "Buffer full, going to sleep for %ld milliseconds", delay);

            bola.placeholderBuffer+= (float)delay/1000.0;
            usleep(delay*1000);
            pthread_mutex_lock(&t->msg_mutex);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            pthread_mutex_unlock(&t->msg_mutex);

        }
        /*No need to wait if the difference is under 10us 
        if ((delay = (segment_start * 1000000) - (gettimelong()-stime + ENCODING_DELAY*1000)) > 10000)
        {
            printdebug(DOWNLOAD, "Encoding Delay: waiting %lld ms \n", delay/1000);

            bola.placeholderBuffer+= (double)delay/1000000.0;
            usleep(delay);
            pthread_mutex_lock(&t->msg_mutex);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            pthread_mutex_unlock(&t->msg_mutex);
        }

        */
        if(buffered_duration< 0)
        {
            /*This shouldn't happen*/
            printdebug(DOWNLOAD,"Getting negative buffered duration, zeroing it");
            buffered_duration = 0 ; 
        }
        
        
        curr_bitrate_level = getMaxIndex(&bola, (float)buffered_duration/1000.0, stime);
        curr_url = m->bitrate_level[curr_bitrate_level].segments[curr_segment];

       // printf("PLAYOUT: ")
        printf("BUFFER: %lld %lld %ld %d %d %ld (%u:%u(%d))\n", (gettimelong()-stime)/1000, t->playout_time, buffered_duration, curr_bitrate_level, curr_segment, m->bitrate_level[curr_bitrate_level].bitrate, curr_offset, end_offset, bytes_rx);
       // printf("\nFinished request for segment %d (Buffer len: %d s). Content len: %d, bytes rx: %d at level : %d\n", curr_segment - 1, buffered_duration/1000 , contentlen, bytes_rx, curr_bitrate_level); fflush(stdout);
        
        //        if (curr_segment % 5 == 0 && curr_bitrate_level > 14)
        //            curr_bitrate_level--;
        
        bytes_rx = 0;
        download_start_time = gettimelong();
        http_resp_len = 0 ;
        if( send_get_request (sock, curr_url, t->Hollywood, segment_start) < 0 )
        {
            perror("ERROR: Send GET request failed on Hollywood\n");
            goto END_DOWNLOAD;
        }
        while (http_resp_len==0)
        {
            http_resp_len = get_html_headers(sock, buf, HTTPHEADERLEN, t->Hollywood, &substream, &new_seq, &curr_offset);
            if( http_resp_len == 0 )
            {
                close(t->sock);
                if((t->sock = connect_tcp_port (t->host, t->port, t->Hollywood, sock, t->OO))<0)
                {
                    printf("ERROR: TCP Connect failed\n");
                    goto END_DOWNLOAD;
                }
                if( send_get_request (sock, curr_url, t->Hollywood, segment_start) < 0 )
                {
                    printf("ERROR: Send GET request failed on Hollywood\n");
                    goto END_DOWNLOAD;
                }
            }
            else if (http_resp_len > 0 && t->Hollywood)
            {
                if (substream != HOLLYWOOD_HTTP_SUBSTREAM){
                if (substream == HOLLYWOOD_DATA_SUBSTREAM_TIMELINED || substream == HOLLYWOOD_DATA_SUBSTREAM_UNTIMELINED)
                {
                    printdebug(DOWNLOAD, "SUBSTREAM %d: We should probably add this to the queue\n", substream);
                    pthread_mutex_lock(&t->msg_mutex);
                    
                    if(is_full(t->rx_buf, new_seq))
                    {
                        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
                    }
                    
                    /*Error code not checked, if message push fails, move on, nothing to do*/
                    if(push_message(t->rx_buf, (uint8_t *)buf, new_seq, http_resp_len)>=0)
                    {
                        bytes_rx += http_resp_len;
                    }
                    pthread_mutex_unlock(&t->msg_mutex);
                    printdebug(DOWNLOAD, "Read %d of %d bytes (seq: %u) \n", bytes_rx, contentlen, new_seq);

                    http_resp_len = 0;
                }
                }
            }
        }
        if(strstr(buf, "200 OK")==NULL)
        {
            printf("Request Failed (resp len: %d): %s \n",  http_resp_len, buf);
            goto END_DOWNLOAD;
        }
        
        contentlen = get_content_length(buf);
        if (contentlen == 0)
        {
            printf("download_segments: Received zero content length, exiting program! \n");
            goto END_DOWNLOAD;
        }
        end_offset = contentlen;
        curr_offset = 0; 
        while ((bytes_rx < contentlen && !t->Hollywood) || ((curr_offset < end_offset ||  bytes_rx < 0.80*contentlen )&& t->Hollywood) )
        {
            if(endnow)
                goto END_DOWNLOAD; 
            
            ret = read_http_body_partial(sock, rx_buf, HOLLYWOOD_MSG_SIZE, t->Hollywood, &new_seq, &curr_offset);
            
            
            if (ret<=0)
            {
                perror("ERROR: download_segments: Socket recv failed: ");
                goto END_DOWNLOAD;
            }
            
            
            pthread_mutex_lock(&t->msg_mutex);
            
            if(is_full(t->rx_buf, new_seq))
            {
                pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
            }
    
            /*Error code not checked, if message push fails, move on, nothing to do*/
            if(push_message(t->rx_buf, rx_buf, new_seq, ret)>=0)
            {
                bytes_rx += ret;

            }
          //  pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
            pthread_mutex_unlock(&t->msg_mutex);
            printdebug(DOWNLOAD, "Read %d of %d bytes (seq: %u) %u: %u\n", bytes_rx, contentlen, new_seq, curr_offset, end_offset);
            
        }
        pthread_cond_signal(&t->msg_ready);

        double download_time = gettimelong() - download_start_time;
        saveThroughput(&bola, (long)((double)bytes_rx*8/(download_time/1000000)));  /*bps*/

        ++curr_segment ;
        
        
    }

END_DOWNLOAD: 
    pthread_mutex_lock(&t->msg_mutex);
    
    printf("Stream has finished downloading %d of %d \n", curr_segment,  m->num_of_segments); fflush(stdout); 

    t->stream_complete = 1;    
    while( !is_empty(t->rx_buf))
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    
    /*Signal continuously to avoid block condition, until the parser has awaken*/
    while(t->init_segment_downloaded == 0)
    {
        pthread_cond_signal(&t->msg_ready);
        pthread_cond_signal(&t->init_ready);
        pthread_mutex_unlock(&t->msg_mutex);
        usleep(10000);
        pthread_mutex_lock(&t->msg_mutex);
    }
    
    pthread_mutex_unlock(&t->msg_mutex);
    
    return 0;
}


int init_transport(transport * t)
{
    t->Hollywood        = 0;
    t->sock             = -1;
    t->stream_complete  = 0;
    t->playout_time     = 0;
    t->OO               = 0;
    t->fptr             = NULL; 
    t->init_segment_downloaded = 0;
    t->parser_exited    = 0; 
    t->rx_buf  = malloc(sizeof(struct playout_buffer));
    memzero(t->rx_buf, sizeof(struct playout_buffer) );
    sprintf(t->host, "");
    sprintf(t->port, "8080");
    return 0; 
}


int play_video (struct metrics * metric, manifest * media_manifest , transport * media_transport, long throughput)
{
    pthread_attr_t attr;

    metric->stime = gettimelong();
    
    /*Initialize the condition and mutex*/
    pthread_cond_init(&media_transport->msg_ready, NULL);
    pthread_cond_init(&media_transport->init_ready, NULL);
    pthread_mutex_init(&media_transport->msg_mutex, NULL);
        
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    int err =pthread_create(&(av_tid), &attr, &mm_parser, (void *)metric);
    
    if(err!=0)
    {
        printf("\ncan't create parser thread :[%s]", strerror(err));
        return -1;
    }

    download_segments(media_manifest, media_transport, metric->stime, throughput);
    
    /*wait for threads to end*/
    pthread_join(av_tid, NULL);
    
    return 0;


}

