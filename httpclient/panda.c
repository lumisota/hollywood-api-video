//
//  panda.c
//  
//
//  Created by Ahsan Saba on 02/08/2017.
//  The code is a slightly modified version of Li Zhi's implementation of PANDA
//  to make it compatible with our own client. 
//
//

#include "panda.h"
/* PANDA constants */
const uint64_t w=700000; /* bps */
const float K=0.7;
const float a=0.4;
const float e=0.1;
const float b=0.3;
const uint64_t B0=12000000; /* us */


void initialize_panda(struct panda_state * panda, manifest * m)
{
    for (int i = 0 ; i < m->num_of_levels; i++)
    {
        panda->bitrates[i] = (double)m->bitrate_level[i].bitrate;
    }
    panda->num_of_levels = m->num_of_levels;
    panda->segment_duration = m->segment_dur;
    
    return;
}

int getMaxBitrateBelowBandwidth(struct panda_state  * panda, long throughput){
    
    int q = 0;
    
    for ( int i = 0; i < panda->num_of_levels; i++)
    {
        if (panda->bitrates[i] <= throughput)
            q = i;
        else
            break;
    }
    return q;
    
}


/* leeoz note: target_bitrate and target_avg_bitrate values are passed out
 * regardless of if PANDA is enabled or not. If enabled, they are the
 * intermediate variables; if disabled, they serve as the transitional
 * parameters when switching from regular algorithm to PANDA.
 tput -> uint64_t tput = segment->size * 8 * 1000000 / __MAX(1, duration); bps
 download_duration -> time to download last segment in seconds
 prev_segment_duration -> duration of last segment in seconds
 buffer_duration -> Length of buffered video in seconds
 */

int BandwidthAdaptation(long tput, struct panda_state * panda,
                               long download_duration,
                               long *target_inter_request_time,
                               long *target_bitrate, long buffer_duration,
                               long *target_avg_bitrate, long *rate_limit,
                               int prev_stream, int panda_enabled)
{
    
    int candidate_stream;
    uint64_t candidate_bitrate;
//    printf("Panda:%d, download_duration: %ld, buffer_duration: %ld, tput: %ld \n", panda_enabled, download_duration, buffer_duration, tput);  
    if (panda_enabled)
    {
        long actual_duration=__MAX(download_duration,panda->segment_duration);
        
        /* get lowest bitrate of stream */
        uint64_t lowest_bitrate= panda->bitrates[0];
        //msg_Info(s, "lowest_bitrate=%"PRIu64"\n",lowest_bitrate);
        
        uint64_t prev_target_bitrate=*target_bitrate;
        
        /* ESTIMATION */
        
        /* below: PANDA v1's AIMD */
        *target_bitrate=(uint64_t)__MAX(0,(float)prev_target_bitrate+
                                        K*(actual_duration/panda->segment_duration)*
                                        (w-__MAX(0,(float)prev_target_bitrate-tput+w)));
        
        /* prevent target bitrate dropping too low */
        if ((float)(*target_bitrate)<((float)lowest_bitrate/(1-e)))
            *target_bitrate=(uint64_t)((float)lowest_bitrate/(1-e));
//        printf("target_bitrate=%"PRIu64"\n",*target_bitrate);
        
        /* SMOOTHING */
        uint64_t prev_target_avg_bitrate=*target_avg_bitrate;
        //    	*target_avg_bitrate =(uint64_t)__MAX(0,((float)prev_target_avg_bitrate -
        //    			   a * ((float)prev_target_avg_bitrate - (float)(*target_bitrate))));
        /* above: time step is seg duration; below: time step is actual step interval */
        *target_avg_bitrate =(uint64_t)__MAX(0,((float)prev_target_avg_bitrate -
                                                a *(actual_duration/panda->segment_duration) *
                                                ((float)prev_target_avg_bitrate - (float)(*target_bitrate))));
        
//        printf("target_avg_bitrate=%"PRIu64"\n",*target_avg_bitrate);
        
        /* leeoz: make sure target_rate can't be lower than
         the actual bitrate of the first seg */
        *target_avg_bitrate=(uint64_t)__MAX((float)(*target_avg_bitrate),
                                            lowest_bitrate);
        
//        printf("target_avg_bitrate=%"PRIu64"\n",*target_avg_bitrate);
        
        /* RATE LIMITING */
        *rate_limit=100000000; /* bit/s */
        //*rate_limit=*target_avg_bitrate; /* bit/s */
        //*rate_limit=(*target_avg_bitrate)+w; /* bit/s */
        //*rate_limit=(uint64_t)__MAX(0,(float)((0.00+1)*(prev_target_bitrate+K*(actual_duration/prev_segment_duration)*w)));
        
        
        
        /* PANDA V1 QUANTIZATION */
        /*90% of target avg. bitrate used for getting the bitrate*/
        int stream_up=getMaxBitrateBelowBandwidth(panda,
                                                  __MAX(0,(uint64_t)((1-e) * ((float)(*target_avg_bitrate)))));
        /*100% of target avg. bitrate used for getting the bitrate, stream_down>stream_up*/
        int stream_down=getMaxBitrateBelowBandwidth(panda,
                                                    __MAX(0,(*target_avg_bitrate)));
        //msg_Info(s, "stream_up=%d,stream_down=%d\n",stream_up,stream_down);
        if(prev_stream < stream_up) {
            candidate_stream=stream_up;
            candidate_bitrate=panda->bitrates[stream_up];
        } else if(prev_stream <= stream_down) {
            candidate_stream=prev_stream;
            candidate_bitrate=panda->bitrates[prev_stream];;
        } else {
            candidate_stream=stream_down;
            candidate_bitrate=panda->bitrates[stream_down];;
        }
        

        
        /* PANDA V1 SCHEDULING */
        *target_inter_request_time=(uint64_t)((float)panda->segment_duration *
                                             candidate_bitrate / (*target_avg_bitrate) +
                                             b * ((float)buffer_duration - B0));
//        printf("target_inter_request_time=%ld\n",*target_inter_request_time);
        
        // Cap max buffer duration
        uint64_t tirt=*target_inter_request_time;
        if(buffer_duration > B0) {
            *target_inter_request_time = panda->segment_duration+buffer_duration-B0;
        }
        //msg_Info(s, "target_inter_request_time=%ld\n",*target_inter_request_time);
        
        /* leeoz record
        char tmp[1028];
        sprintf(tmp,"now=%.1f | %.4f => %.4f Mbps | %.3f %.3f [%.3f] %.3f Mbps | %.3f [%.3f] %.3f Mbps | %.1f [%.1f] Sec | %.1f %.1f(%.1f) Sec | k=%.4f w=%.4f a=%.4f e=%.4f b=%.4f B0=%.4f",
                fmod((float)mdate()/1.0e6,1.0e3),
                prev_hls->bandwidth/1.0e6,candidate_bitrate/1.0e6,
                prev_target_bitrate/1.0e6,tput/1.0e6,((float)(*target_bitrate)-prev_target_bitrate)/1.0e6,*target_bitrate/1.0e6,
                prev_target_avg_bitrate/1.0e6,((float)(*target_avg_bitrate)-prev_target_avg_bitrate)/1.0e6,*target_avg_bitrate/1.0e6,
                buffer_duration/1.0e6, (buffer_duration-B0)/1.0e6,
                segment_duration/1.0e6, tirt/1.0e6, *target_inter_request_time/1.0e6,
                K,w/1.0e6,a,e,b,B0/1.0e6
                );
        msg_Info(s, "PANDA epoch: %s",tmp); */
    }
    else{
        candidate_stream=getMaxBitrateBelowBandwidth(panda,__MAX(0,tput));

        if (buffer_duration>B0)
            *target_inter_request_time = panda->segment_duration;
        else
            *target_inter_request_time = 0; /* leeoz modify */
        
        *target_bitrate=panda->bitrates[candidate_stream];
        *target_avg_bitrate=panda->bitrates[candidate_stream];

    }
    
        

    
    return candidate_stream;
}
