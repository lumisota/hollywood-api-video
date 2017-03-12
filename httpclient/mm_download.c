//
//  mm_download.c
//  
//
//  Created by Saba Ahsan on 22/02/17.
//
//

#include "mm_download.h"

pthread_t       av_tid;          /*thread id of the av parser thread*/





int download_segments_tcp( manifest * m, transport * t )
{
    char buf[HTTPHEADERLEN];
    uint8_t rx_buf[HOLLYWOOD_MSG_SIZE] = {0};
    int ret                         = 0;
    int bytes_rx                    = 0;
    int contentlen                  = 0;
    int curr_segment                = 0;
    int curr_bitrate_level          = 0;
    char * curr_url                 = NULL;
    int http_resp_len               = 0;

    while (curr_segment != m->num_of_segments )
    {
        printf("Finished request for segment %d. Content len: %d, bytes rx: %d at level : %d\n", curr_segment - 1, contentlen, bytes_rx, curr_bitrate_level); fflush(stdout);
        
//        if (curr_segment % 5 == 0 && curr_bitrate_level > 14)
//            curr_bitrate_level--;
        
        
        bytes_rx = 0;
        curr_url = m->bitrate_level[curr_bitrate_level].segments[curr_segment];

        http_resp_len = 0 ;

        while (http_resp_len==0)
        {
            if( send_get_request ( &t->sock, curr_url, 0) < 0 )
                break;
        
            http_resp_len = get_html_headers(&t->sock, buf, HTTPHEADERLEN, t->Hollywood);
            if( http_resp_len == 0 )
            {
                close(t->sock);
                if((t->sock = connect_tcp_port (t->host, t->port))<0)
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
            printf("Received zero content length, exiting program! \n");
            break;
        }
        
        while (bytes_rx < contentlen )
        {
            
            ret = recv(t->sock, rx_buf, HOLLYWOOD_MSG_SIZE, 0);
            
            /*Write buffer to file for later use*/
            if(ret>0)
            {
                if(fwrite (rx_buf , sizeof(uint8_t), ret, t->fptr)!=ret)
                {
                    if (ferror (t->fptr))
                        printf ("Error Writing to file\n");
                    perror("File writing error occured: ");
                    return -1;
                }
                
            }
            else if (ret<0)
            {
                perror("Socket recv failed: ");
                return -1;
            }
            
            bytes_rx += ret;
            
            pthread_mutex_lock(&t->msg_mutex);
            
            if( t->rx_buf != NULL)
                pthread_cond_wait( &t->msg_ready, &t->msg_mutex );

            t->rx_buf = rx_buf;
            t->buf_len = ret;
            
            pthread_cond_signal(&t->msg_ready);
            pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
            pthread_mutex_unlock(&t->msg_mutex);

        }
        ++curr_segment ;

        
    }
    pthread_mutex_lock(&t->msg_mutex);
    
    printf("Stream has finished downloading\n");
    
    if( t->rx_buf != NULL)
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    
    t->stream_complete = 1;
    
    pthread_cond_signal(&t->msg_ready);
    pthread_mutex_unlock(&t->msg_mutex);
    
    return 0;
}


int init_transport(transport * t)
{
    t->Hollywood        = 0;
    t->sock             = -1;
    t->fptr             = NULL;
    t->rx_buf           = NULL;
    t->buf_len          = 0;
    t->packets_queued   = 0;
    t->lowest_seq_num   = 0;
    t->stream_complete  = 0;
    sprintf(t->host, "");
    sprintf(t->port, "8080");
    return 0; 
}


int play_video (struct metrics * metric, manifest * media_manifest , transport * media_transport)
{
    pthread_attr_t attr;

    metric->stime = gettimelong();
    printf("Starting download 1\n"); fflush(stdout);
    
    
    /*Initialize the condition and mutex*/
    pthread_cond_init(&media_transport->msg_ready, NULL);
    pthread_mutex_init(&media_transport->msg_mutex, NULL);
    
    pthread_mutex_init(&metric->av_mutex, NULL);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    int err =pthread_create(&(av_tid), &attr, &mm_parser, (void *)metric);
    
    if(err!=0)
    {
        printf("\ncan't create parser thread :[%s]", strerror(err));
        return -1;
    }

    download_segments_tcp(media_manifest, media_transport);
    
    /*wait for threads to end*/
    pthread_join(av_tid, NULL);
    
    return 0;


}

