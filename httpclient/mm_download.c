//
//  mm_download.c
//  
//
//  Created by Saba Ahsan on 22/02/17.
//
//

#include "mm_download.h"

pthread_t       av_tid;          /*thread id of the av parser thread*/
#define BUFFER_DURATION 5000 /*seconds*/
#define IS_DYNAMIC      1

//#define DOWNLOAD "DOWNLOAD"
#define DOWNLOAD ""

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
    long long start_time            = 0;
    void * sock;
    long buffered_duration          = 0;
    
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
    while (curr_segment <= m->num_of_segments )
    {
        if(curr_segment == 0)
            buffered_duration = 0;
        else
        {
            pthread_mutex_lock(&t->msg_mutex);
            buffered_duration = (m->segment_dur * (curr_segment - m->init) * 1000) - t->playout_time;
            fflush(stdout);
            pthread_mutex_unlock(&t->msg_mutex);
        }
        
        
        if(buffered_duration > BUFFER_DURATION)
        {
            /*Delay due to bufferLevel > bufferTarget is added to BOLA placeholder buffer*/
            long delay =(buffered_duration-BUFFER_DURATION);
            printdebug(DOWNLOAD, "Buffer full, going to sleep for %ld seconds", delay);

            bola.placeholderBuffer+= (float)delay/1000.0;
            usleep(delay*1000);
            pthread_mutex_lock(&t->msg_mutex);
            buffered_duration = (m->segment_dur * (curr_segment - m->init) * 1000) - t->playout_time;
            pthread_mutex_unlock(&t->msg_mutex);

        }
        if(buffered_duration< 0)
        {
            /*This shouldn't happen*/
            printdebug(DOWNLOAD,"Getting negative buffered duration, zeroing it");
            buffered_duration = 0 ; 
        }
        curr_bitrate_level = getMaxIndex(&bola, (float)buffered_duration/1000.0, stime);
        curr_url = m->bitrate_level[curr_bitrate_level].segments[curr_segment];

       // printf("PLAYOUT: ")
        printf("BUFFER: %lld %lld %ld %d %d %ld\n", (gettimelong()-stime)/1000, t->playout_time, buffered_duration, curr_bitrate_level, curr_segment, m->bitrate_level[curr_bitrate_level].bitrate);
       // printf("\nFinished request for segment %d (Buffer len: %d s). Content len: %d, bytes rx: %d at level : %d\n", curr_segment - 1, buffered_duration/1000 , contentlen, bytes_rx, curr_bitrate_level); fflush(stdout);
        
        //        if (curr_segment % 5 == 0 && curr_bitrate_level > 14)
        //            curr_bitrate_level--;
        
        bytes_rx = 0;
        
        http_resp_len = 0 ;
        
        while (http_resp_len==0)
        {
            if( send_get_request (sock, curr_url, t->Hollywood) < 0 )
                break;
            
            http_resp_len = get_html_headers(sock, buf, HTTPHEADERLEN, t->Hollywood);
            if( http_resp_len == 0 )
            {
                close(t->sock);
                if((t->sock = connect_tcp_port (t->host, t->port, t->Hollywood, sock, t->OO))<0)
                    break;
            }
            
        }
        
        if(strstr(buf, "200 OK")==NULL)
        {
            printf("Request Failed: %s \n", buf);
            break;
        }
        
        contentlen = get_content_length(buf);
        if (contentlen == 0)
        {
            printf("download_segments: Received zero content length, exiting program! \n");
            break;
        }
        start_time = gettimelong();
        
        while (bytes_rx < contentlen )
        {
            if(endnow)
                goto END_DOWNLOAD; 
            
            ret = read_http_body_partial(sock, rx_buf, HOLLYWOOD_MSG_SIZE, t->Hollywood, &new_seq, NULL);
            
            
            /*Write buffer to file for later use*/
            if(ret>0)
            {
                if(fwrite (rx_buf , sizeof(uint8_t), ret, t->fptr)!=ret)
                {
                    if (ferror (t->fptr))
                        printf ("download_segments: Error Writing to file\n");
                    perror("File writing error occured: ");
                    return -1;
                }
                
            }
            else if (ret<0)
            {
                perror("download_segments: Socket recv failed: ");
                return -1;
            }
            else
            {
                printf("download_segments: Received 0 bytes, connection closed\n");
            }
            
            bytes_rx += ret;
            
            pthread_mutex_lock(&t->msg_mutex);
            
            if(is_full(t->rx_buf, new_seq))
            {
                pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
            }
    
            /*Error code not checked, if message push fails, move on, nothing to do*/
            push_message(t->rx_buf, rx_buf, new_seq, ret);

            pthread_cond_signal(&t->msg_ready);
          //  pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
            pthread_mutex_unlock(&t->msg_mutex);
 //           printf("Read %d of %d bytes \n", bytes_rx, contentlen);
            
        }
        double download_time = gettimelong() - start_time;
        saveThroughput(&bola, (long)((double)bytes_rx*8/(download_time/1000000)));  /*bps*/
        ++curr_segment ;
        
        
    }

END_DOWNLOAD: 
    pthread_mutex_lock(&t->msg_mutex);
    
    printf("Stream has finished downloading %d of %d \n", curr_segment,  m->num_of_segments);

    t->stream_complete = 1;    
    while( !is_empty(t->rx_buf))
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    
//    t->stream_complete = 1;
    
    pthread_cond_signal(&t->msg_ready);
    pthread_mutex_unlock(&t->msg_mutex);
    
    return 0;
}


int init_transport(transport * t)
{
    t->Hollywood        = 0;
    t->sock             = -1;
    t->fptr             = NULL;
    t->stream_complete  = 0;
    t->playout_time     = 0;
    t->OO               = 0; 
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

