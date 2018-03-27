//
//  mm_download.c
//  
//
//  Created by Saba Ahsan on 22/02/17.
//
//

#include "mm_download.h"


using namespace dash::http;

pthread_t       av_tid;          /*thread id of the av parser thread*/
#define IS_DYNAMIC      1
#define ENCODING_DELAY  3000
//#define DOWNLOAD "DOWNLOAD"
#define DOWNLOAD ""
static uint min_buffer_len = 1000; 

extern int endnow; 
extern int buffer_dur_ms; 
extern float min_rxcontent_ratio;

int add_to_queue(uint8_t * buf, uint32_t len, transport * t, uint32_t new_seq)
{
    int ret = 0; 
    pthread_mutex_lock(&t->msg_mutex);
                    
    if(is_full(t->rx_buf, new_seq))
    {
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    }
                    
    /*Error code not checked, if message push fails, move on, nothing to do*/
    if(push_message(t->rx_buf, (uint8_t *)buf, new_seq, len)>0)
    {
        pthread_cond_signal(&t->msg_ready);
        ret = len;
    }
    pthread_mutex_unlock(&t->msg_mutex);
    return ret; 
}


int monitor_socket_for_delayed_packets(void * sock, char * buf, int len,  transport * t, long long * delay_ms, double * download_time, long long download_start_time){

    uint8_t substream            = 0;
    uint32_t seq                    = 0;
    uint64_t offset                 = 0;
    long long exit_time             = gettimelong() + (*delay_ms * 1000);
    int ret                         = 0; 
    int timeout_s                   = (exit_time - gettimelong())/1000000;
    int bytes_rx                    = 0;
    //    printf("Monitroring for delayed packets\n");fflush(stdout);  
    while (timeout_s > 0)
    {
        ret = recv_message((hlywd_sock * )sock, buf, len, 0, &substream, timeout_s);
        if( ret == -2 ) {
          //  printf("Select monitor timed out (bytes rx: %d) \n", bytes_rx); 
            return bytes_rx; 
        }
        else if (ret>0) {
            if (substream == HOLLYWOOD_DATA_SUBSTREAM_TIMELINED || substream == HOLLYWOOD_DATA_SUBSTREAM_UNTIMELINED)
            {
                ret -= HLYWD_MSG_TRAILER;
                if(ret<0)
                {
                    printf("ERROR: Received Hollywood message too short while reading HTTP header\n");
                    exit(1);
                }

                offset = atouint64 ((unsigned char *)buf+ret);;
                seq = atouint32 ((unsigned char *)buf+ret+sizeof(offset));
 
                printdebug(DOWNLOAD, "SUBSTREAM %d: Received packets while in wait condition\n", substream);
                int i = add_to_queue((unsigned char *)buf, ret, t, seq);
                if (i > 0)
                {
                    bytes_rx += i; 
                    *download_time = gettimelong() - download_start_time; 
                }

                timeout_s = (exit_time - gettimelong())/1000000;
                if (*delay_ms/1000>timeout_s)
                    *delay_ms = *delay_ms - (timeout_s*1000); 
            }
        }
        else 
            return -1; 
    }
    
    return bytes_rx; 

}

int download_segments_abmap( manifest * m, transport * t , long long stime)
{
    char buf[HTTPHEADERLEN];
    uint8_t rx_buf[HOLLYWOOD_MSG_SIZE] = {0};
    int ret                         = 0;
    int bytes_rx                    = t->bytes_rx;
    int contentlen                  = 0;
    int curr_segment                = 0;
    int curr_bitrate_level          = 1;
    char * curr_url                 = NULL;
    int http_resp_len               = 0;
    uint32_t new_seq                = 0;
    long long download_start_time   = 0;
    void * sock                     = NULL;
    long buffered_duration          = 0;
    uint8_t substream               = 0;
    int segment_start               = 0;
    long long delay                 = 0;
    uint64_t end_offset             = 0;
    uint64_t curr_offset            = 0;
    uint64_t highest_offset         = 0;
    double download_time            = t->download_time;
    uint64_t rtt                    = t->rtt;
    AdaptationManagerABMAplus * buffMgr;
    uint8_t loss_alert              = 0;

    if ( t->Hollywood) {
        sock = &(t->h_sock);
    }
    else {
        sock = &(t->sock);
    }
    
    std::vector<uint64_t> repVector;
    for (int i = 0 ; i < m->num_of_levels; i++)
    {
        repVector.push_back((uint64_t) m->bitrate_level[i].bitrate);
    }
    printf("Segment Duration: %ld\n", m->segment_dur_ms); 

    buffMgr = new AdaptationManagerABMAplus(repVector, m->segment_dur_ms*1000000, (uint64_t)buffer_dur_ms*1000, 2000000, 10, 50, 0.1 );

    while (curr_segment < m->num_of_segments ){
        
        if(curr_segment == 0) {
            segment_start = 0;
            buffered_duration = 0;
        }
        else {
            pthread_mutex_lock(&t->msg_mutex);
            if(t->parser_exited) {
                t->stream_complete = 1;
                pthread_mutex_unlock(&t->msg_mutex);
                goto END_DOWNLOAD;
            }
            loss_alert = t->loss_alert;
            t->loss_alert = 0;

            if(t->p_status == P_READY)
            {
                pthread_cond_signal(&t->queue_ready);
            }
            if(t->p_status==P_STARTUP)
                t->p_status=P_STANDBY;
            else if(t->p_status==P_STANDBY)
                t->p_status=P_READY;
            segment_start = m->segment_dur_ms * (curr_segment - m->init);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            //printf("Buffered Duration %ld, (segment_start %ld, playout_time %ld)\n",  buffered_duration,segment_start, t->playout_time);
            fflush(stdout);
            pthread_mutex_unlock(&t->msg_mutex);

        }
    
        if((delay = (buffered_duration - buffer_dur_ms)) >= 1000 ) {
            /*Delay due to bufferLevel > bufferTarget is added to BOLA placeholder buffer*/
            printdebug(DOWNLOAD, "Buffer full, going to sleep for %ld milliseconds", delay);
            if ( t->Hollywood) {
                long long tmp_delay = delay;
                int i = monitor_socket_for_delayed_packets(sock, (char *)rx_buf, HOLLYWOOD_MSG_SIZE, t, &tmp_delay, &download_time, download_start_time);
                if (i > 0)
                    bytes_rx+=i;
            }
            else
            {
                usleep(delay*1000);
            }
            pthread_mutex_lock(&t->msg_mutex);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            pthread_mutex_unlock(&t->msg_mutex);
            
        }
        
        buffMgr->addRtt(rtt);
        if(loss_alert)
            buffMgr->addData(bytes_rx, download_time+rtt, buffered_duration);
        else
            buffMgr->addData(bytes_rx, download_time, buffered_duration);
        buffMgr->adaptation();
        curr_bitrate_level = buffMgr->getRepresentationIdx();
        curr_url = m->bitrate_level[curr_bitrate_level].segments[curr_segment];
        

        
        if(buffered_duration < 0) {
            /*This shouldn't happen*/
            printdebug(DOWNLOAD,"Getting negative buffered duration, zeroing it");
            buffered_duration = 0 ;
        }
        
        printf("BUFFER: %lld %lld %ld %d %d %ld (%llu:%llu (%d of %d)) %d %d\n", (gettimelong()-stime)/1000, t->playout_time, buffered_duration, curr_bitrate_level, curr_segment, m->bitrate_level[curr_bitrate_level].bitrate, curr_offset, end_offset, bytes_rx, contentlen, loss_alert, t->rx_buf->late_or_duplicate_packets);
        
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
                if((t->sock = connect_tcp_port (t->host, t->port, t->Hollywood, sock, t->OO))<0) {
                    printf("ERROR: TCP Connect failed\n");
                    goto END_DOWNLOAD;
                }
                if( send_get_request (sock, curr_url, t->Hollywood, segment_start) < 0 ) {
                    printf("ERROR: Send GET request failed on Hollywood\n");
                    goto END_DOWNLOAD;
                }
            }
            else if (http_resp_len > 0 && t->Hollywood) {
                if (substream != HOLLYWOOD_HTTP_SUBSTREAM){
                    if (substream == HOLLYWOOD_DATA_SUBSTREAM_TIMELINED || substream == HOLLYWOOD_DATA_SUBSTREAM_UNTIMELINED) {
                        bytes_rx += add_to_queue((unsigned char *)buf, http_resp_len, t, new_seq);
                        http_resp_len = 0;
                    }
                }
            }
        }
        rtt = gettimelong() - download_start_time;
        if ((contentlen = get_content_length(buf)) == 0) {
            printf("HTTP Response error: \n%s\n", buf);
            goto END_DOWNLOAD;
        }
        
        end_offset += contentlen;
        while ((bytes_rx < contentlen && !t->Hollywood) || ((highest_offset < end_offset ||  bytes_rx < min_rxcontent_ratio*contentlen )&& t->Hollywood) )
        {
            if(endnow)
                goto END_DOWNLOAD;
            
            ret = read_http_body_partial(sock, rx_buf, HOLLYWOOD_MSG_SIZE, t->Hollywood, &new_seq, &curr_offset);
            //printf("2. Received packet size %d, offset %" PRIu64 ", seq %u\n", ret, curr_offset, new_seq);
            if(highest_offset<curr_offset)
                highest_offset = curr_offset;
            if(ret==-2)
            {
                printdebug(DOWNLOAD,"Timeout occurred while receiving for HTTP body\n");
                break;
            }
            else if (ret<=0)
            {
                perror("ERROR: download_segments: Socket recv failed: ");
                goto END_DOWNLOAD;
            }
            
            
            bytes_rx += add_to_queue((unsigned char *)rx_buf, ret, t, new_seq);
            
        }
        
        download_time = gettimelong() - download_start_time;
                
        ++curr_segment ;
        
    }
    
    
END_DOWNLOAD:
    delete buffMgr;
    pthread_mutex_lock(&t->msg_mutex);
    
    printf("Stream has finished downloading %d of %d \n", curr_segment,  m->num_of_segments); fflush(stdout);
    
    t->stream_complete = 1;
    while( !is_empty(t->rx_buf))
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    
    /*Signal continuously to avoid block condition, until the parser has awaken*/
    while( t->parser_exited == 0 )
    {
        pthread_cond_signal(&t->msg_ready);
        pthread_cond_signal(&t->queue_ready);
        pthread_mutex_unlock(&t->msg_mutex);
        usleep(10000);
        pthread_mutex_lock(&t->msg_mutex);
    }
    
    pthread_mutex_unlock(&t->msg_mutex);
    
    return 0;
}


int download_segments_panda( manifest * m, transport * t , long long stime, long throughput)
{
    char buf[HTTPHEADERLEN];
    struct panda_state panda          = {{0}};
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
    void * sock                     = NULL;
    long buffered_duration          = 0;
    uint8_t substream               = 0;
    int segment_start               = 0;
    long long delay                 = 0;
    uint64_t end_offset             = 0;
    uint64_t curr_offset            = 0;
    uint64_t highest_offset         = 0;
    double download_time            = 0.0;
    int panda_enabled               = 0;
    long target_inter_request_time = 0;
    long target_bitrate = 500000; /* bit/s */
    long target_avg_bitrate = 500000; /* bit/s */
    long rate_limit = 100000000; /* bit/s */
    uint8_t loss_alert              = 0;
    
    if ( t->Hollywood) {
        sock = &(t->h_sock);
    }
    else {
        sock = &(t->sock);
    }
    
    initialize_panda(&panda, m, (uint64_t)buffer_dur_ms*1000);
    while (curr_segment < m->num_of_segments ){
        
        if(curr_segment == 0) {
            segment_start = 0;
            buffered_duration = 0;
        }
        else {
            pthread_mutex_lock(&t->msg_mutex);
            if(t->parser_exited) {
                t->stream_complete = 1;
                pthread_mutex_unlock(&t->msg_mutex);
                goto END_DOWNLOAD;
            }
            loss_alert = t->loss_alert;
            t->loss_alert = 0;
            if(t->p_status == P_READY)
            {
                pthread_cond_signal(&t->queue_ready);
                panda_enabled = 1;
            }
            if(t->p_status==P_STARTUP)
                t->p_status=P_STANDBY;
            else if(t->p_status==P_STANDBY)
                t->p_status=P_READY; 
            segment_start = m->segment_dur_ms * (curr_segment - m->init);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            //printf("Buffered Duration %ld, (segment_start %ld, playout_time %ld)\n",  buffered_duration,segment_start, t->playout_time);
            fflush(stdout);
            pthread_mutex_unlock(&t->msg_mutex);
        }
        if(loss_alert)
            curr_bitrate_level = BandwidthAdaptation(throughput, &panda,
                                                 download_time/1000000,
                                                 &target_inter_request_time,
                                                 &target_bitrate, 0,
                                                 &target_avg_bitrate, &rate_limit,
                                                 curr_bitrate_level, panda_enabled);
        else
            curr_bitrate_level = BandwidthAdaptation(throughput, &panda,
                                                 download_time/1000000,
                                                 &target_inter_request_time,
                                                 &target_bitrate, buffered_duration/1000,
                                                 &target_avg_bitrate, &rate_limit,
                                                 curr_bitrate_level, panda_enabled);
        curr_url = m->bitrate_level[curr_bitrate_level].segments[curr_segment];
        
        if(target_inter_request_time >= 1000 ) {
                    /*Delay due to bufferLevel > bufferTarget is added to BOLA placeholder buffer*/
            printdebug(DOWNLOAD, "Buffer full, going to sleep for %ld milliseconds", delay);
            if ( t->Hollywood) {
                long long tmp_delay = delay;
                int i = monitor_socket_for_delayed_packets(sock, (char *)rx_buf, HOLLYWOOD_MSG_SIZE, t, &tmp_delay, &download_time, download_start_time);
                if (i > 0)
                    bytes_rx+=i;
                
            }
            else
            {
                usleep(delay*1000); 
            }
            pthread_mutex_lock(&t->msg_mutex);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            pthread_mutex_unlock(&t->msg_mutex);

        }

        if(buffered_duration < 0) {
            /*This shouldn't happen*/
            printdebug(DOWNLOAD,"Getting negative buffered duration, zeroing it");
            buffered_duration = 0 ;
        }
        printf("BUFFER: %lld %lld %ld %d %d %ld (%llu:%llu (%d of %d)) %d %d\n", (gettimelong()-stime)/1000, t->playout_time, buffered_duration, curr_bitrate_level, curr_segment, m->bitrate_level[curr_bitrate_level].bitrate, curr_offset, end_offset, bytes_rx, contentlen, loss_alert, t->rx_buf->late_or_duplicate_packets);
        
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
                if((t->sock = connect_tcp_port (t->host, t->port, t->Hollywood, sock, t->OO))<0) {
                    printf("ERROR: TCP Connect failed\n");
                    goto END_DOWNLOAD;
                }
                if( send_get_request (sock, curr_url, t->Hollywood, segment_start) < 0 ) {
                    printf("ERROR: Send GET request failed on Hollywood\n");
                    goto END_DOWNLOAD;
                }
            }
            else if (http_resp_len > 0 && t->Hollywood) {
                if (substream != HOLLYWOOD_HTTP_SUBSTREAM){
                    if (substream == HOLLYWOOD_DATA_SUBSTREAM_TIMELINED || substream == HOLLYWOOD_DATA_SUBSTREAM_UNTIMELINED) {
                        bytes_rx += add_to_queue((unsigned char *)buf, http_resp_len, t, new_seq);
                        http_resp_len = 0;
                    }
                }
            }
        }
        
        if ((contentlen = get_content_length(buf)) == 0) {
            printf("HTTP Response error: \n%s\n", buf);
            goto END_DOWNLOAD;
        }
        
        end_offset += contentlen;
        while ((bytes_rx < contentlen && !t->Hollywood) || ((highest_offset < end_offset ||  bytes_rx < min_rxcontent_ratio*contentlen )&& t->Hollywood) )
        {
            if(endnow)
                goto END_DOWNLOAD; 
            
            ret = read_http_body_partial(sock, rx_buf, HOLLYWOOD_MSG_SIZE, t->Hollywood, &new_seq, &curr_offset);
            //printf("2. Received packet size %d, offset %" PRIu64 ", seq %u\n", ret, curr_offset, new_seq); 
            if(highest_offset<curr_offset)
                highest_offset = curr_offset;
            if(ret==-2)
            {
                printdebug(DOWNLOAD,"Timeout occurred while receiving for HTTP body\n"); 
                break;
            }
            else if (ret<=0)
            {
                perror("ERROR: download_segments: Socket recv failed: ");
                goto END_DOWNLOAD;
            }
            
            
            bytes_rx += add_to_queue((unsigned char *)rx_buf, ret, t, new_seq);
            
        }

        download_time = gettimelong() - download_start_time;

        throughput =  (long)((double)bytes_rx*8/(download_time/1000000));
        
        

        ++curr_segment ;

    }

    
END_DOWNLOAD:
    pthread_mutex_lock(&t->msg_mutex);
    
    printf("Stream has finished downloading %d of %d \n", curr_segment,  m->num_of_segments); fflush(stdout); 

    t->stream_complete = 1;    
    while( !is_empty(t->rx_buf))
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    
    /*Signal continuously to avoid block condition, until the parser has awaken*/
    while( t->parser_exited == 0 )
    {
        pthread_cond_signal(&t->msg_ready);
        pthread_cond_signal(&t->queue_ready);
        pthread_mutex_unlock(&t->msg_mutex);
        usleep(10000);
        pthread_mutex_lock(&t->msg_mutex);
    }
    
    pthread_mutex_unlock(&t->msg_mutex);
    
    return 0;
}



int download_segments_bola( manifest * m, transport * t , long long stime, long throughput)
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
    long long delay                 = 0;
    uint64_t end_offset             = 0;
    uint64_t curr_offset            = 0;
    uint64_t highest_offset         = 0;
    double download_time            = 0.0;
    uint8_t loss_alert              = 0;
    long long last_seq_last_chunk   = 0;
    long long bytes_rx_this_chunk   = 0;
    float buffered_duration_in_sec  = 0.0;
    
    /*Initialize bola, isDynamic is set to 1 (Live)*/
    curr_bitrate_level = calculateInitialState(m, IS_DYNAMIC, &bola);
    saveThroughput(&bola, throughput);  /*bps*/

    if ( t->Hollywood) {
        sock = &(t->h_sock);
    }
    else {
        sock = &(t->sock);
    }
    
    printf("\n");
    while (curr_segment < m->num_of_segments ){
    
        if(curr_segment == 0) {
            segment_start = 0;
            buffered_duration = 0;
        }
        else {
            pthread_mutex_lock(&t->msg_mutex);
            if(t->parser_exited) {
                t->stream_complete = 1;
                pthread_mutex_unlock(&t->msg_mutex);
                goto END_DOWNLOAD;
            }
            loss_alert = t->loss_alert;
            t->loss_alert = 0;
            segment_start = m->segment_dur_ms * (curr_segment - m->init);
            buffered_duration = (segment_start * 1000) - t->playout_time;
            if(t->p_status==P_STARTUP && buffered_duration > min_buffer_len)
                t->p_status=P_READY;
            if(t->p_status == P_READY)
            {
                printf("Signalling condition\n"); 
                pthread_cond_signal(&t->queue_ready);
            }
            pthread_mutex_unlock(&t->msg_mutex);
        }

        if((delay = (buffered_duration - buffer_dur_ms)) >= 1000 ) {
            /*Delay due to bufferLevel > bufferTarget is added to BOLA placeholder buffer*/
            printdebug(DOWNLOAD, "Buffer full, going to sleep for %ld milliseconds", delay);
            if ( t->Hollywood) {
		printf("Delay before: %lld, ", delay); 
                int i = monitor_socket_for_delayed_packets(sock, (char *)rx_buf, HOLLYWOOD_MSG_SIZE, t, &delay, &download_time, download_start_time);
		printf("Delay after: %lld ", delay); 
                bola.placeholderBuffer+= (float)delay/1000.0; 
            }
            else
            {
                usleep(delay*1000);
                bola.placeholderBuffer+= (float)delay/1000.0;
            }
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
        if(buffered_duration< 0) {
            /*This shouldn't happen*/
            printdebug(DOWNLOAD,"Getting negative buffered duration, zeroing it");
            buffered_duration = 0 ; 
        }

        saveThroughput(&bola, (long)((double)bytes_rx*8/(download_time/1000000)));  /*bps*/
        buffered_duration_in_sec = (float)buffered_duration/1000.0; 
        if(loss_alert && buffered_duration_in_sec >= (float)m->segment_dur_ms )
	{
            buffered_duration_in_sec = buffered_duration_in_sec - (float)m->segment_dur_ms; 
            printf("Setting Buffered duration to %f\n ", buffered_duration_in_sec);
	}
        curr_bitrate_level = getMaxIndex(&bola, buffered_duration_in_sec, stime);

        curr_url = m->bitrate_level[curr_bitrate_level].segments[curr_segment];

        printf("BUFFER: %lld %lld %ld %d %d %ld (%llu:%llu (%d of %d)) %d %d %lld\n", (gettimelong()-stime)/1000, t->playout_time, buffered_duration, curr_bitrate_level, curr_segment, m->bitrate_level[curr_bitrate_level].bitrate, curr_offset, end_offset, bytes_rx, contentlen, loss_alert, t->rx_buf->late_or_duplicate_packets,(long)((double)bytes_rx*8/(download_time/1000000)));
        if(t->loss_alert)
            t->loss_alert = 0;

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
                if((t->sock = connect_tcp_port (t->host, t->port, t->Hollywood, sock, t->OO))<0) {
                    printf("ERROR: TCP Connect failed\n");
                    goto END_DOWNLOAD;
                }
                if( send_get_request (sock, curr_url, t->Hollywood, segment_start) < 0 ) {
                    printf("ERROR: Send GET request failed on Hollywood\n");
                    goto END_DOWNLOAD;
                }
            }
            else if (http_resp_len > 0 && t->Hollywood) {
                if (substream != HOLLYWOOD_HTTP_SUBSTREAM){
                    if (substream == HOLLYWOOD_DATA_SUBSTREAM_TIMELINED || substream == HOLLYWOOD_DATA_SUBSTREAM_UNTIMELINED) {
                        bytes_rx += add_to_queue((unsigned char *)buf, http_resp_len, t, new_seq);
                        http_resp_len = 0;
                    }
                }
            }
        }
        
        if ((contentlen = get_content_length(buf)) == 0) {
            printf("HTTP Response error: \n%s\n", buf);
            goto END_DOWNLOAD;
        }
        end_offset += contentlen;
        bytes_rx_this_chunk = 0;
        while ((bytes_rx < contentlen && !t->Hollywood) || ((bytes_rx_this_chunk < min_rxcontent_ratio*contentlen )&& t->Hollywood) )
        {
            if(endnow)
                goto END_DOWNLOAD; 
            
            ret = read_http_body_partial(sock, rx_buf, HOLLYWOOD_MSG_SIZE, t->Hollywood, &new_seq, &curr_offset);
            //printf("2. Received packet size %d, offset %" PRIu64 ", seq %u\n", ret, curr_offset, new_seq); 

            if(ret==-2)
            {
                printdebug(DOWNLOAD,"Timeout occurred while receiving for HTTP body\n"); 
                break;
            }
            else if (ret<=0)
            {
                perror("ERROR: download_segments: Socket recv failed: ");
                goto END_DOWNLOAD;
            }
            
            
            ret = add_to_queue((unsigned char *)rx_buf, ret, t, new_seq);
            bytes_rx += ret;
            if(new_seq >last_seq_last_chunk)
                bytes_rx_this_chunk+=ret;
            
        }
        last_seq_last_chunk = ceil(contentlen/HOLLYWOOD_MSG_SIZE);
        download_time = gettimelong() - download_start_time;


        ++curr_segment ;

            
        
    }

END_DOWNLOAD: 
    pthread_mutex_lock(&t->msg_mutex);
    
    printf("Stream has finished downloading %d of %d \n", curr_segment,  m->num_of_segments); fflush(stdout); 

    t->stream_complete = 1;    
    while( !is_empty(t->rx_buf))
        pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    
    /*Signal continuously to avoid block condition, until the parser has awaken*/
    while( t->parser_exited == 0 )
    {
        pthread_cond_signal(&t->msg_ready);
        pthread_cond_signal(&t->queue_ready);
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
    t->algo             = 0;
    t->stream_complete  = 0;
    t->playout_time     = 0;
    t->OO               = 0;
    t->fptr             = NULL; 
    t->p_status = P_STARTUP;    
    t->parser_exited    = 0;
    t->loss_alert       = 0;
    t->rx_buf  = (struct playout_buffer *)malloc(sizeof(struct playout_buffer));
    memzero(t->rx_buf, sizeof(struct playout_buffer) );
    sprintf(t->host, "");
    sprintf(t->port, "8080");
    return 0; 
}


int play_video (struct metrics * metric, manifest * media_manifest , transport * media_transport, long throughput)
{
    pthread_attr_t attr;

    metric->stime = gettimelong();
    min_buffer_len = metric->minbufferlen;

    /*Initialize the condition and mutex*/
    pthread_cond_init(&media_transport->msg_ready, NULL);
    pthread_cond_init(&media_transport->queue_ready, NULL);
    pthread_mutex_init(&media_transport->msg_mutex, NULL);
        
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    int err =pthread_create(&(av_tid), &attr, &mm_parser, (void *)metric);
    
    if(err!=0)
    {
        printf("\ncan't create parser thread :[%s]", strerror(err));
        return -1;
    }
    
    if(media_transport->algo == 0) /*BOLA*/
    {
        download_segments_bola(media_manifest, media_transport, metric->stime, throughput);
    }
    else if(media_transport->algo == 1) /*PANDA*/
    {
        download_segments_panda(media_manifest, media_transport, metric->stime, throughput);
    }
    else if(media_transport->algo == 2) /*ABMAP*/
    {
        download_segments_abmap(media_manifest, media_transport, metric->stime);
    }

    /*wait for threads to end*/
    pthread_join(av_tid, NULL);
    
    return 0;


}