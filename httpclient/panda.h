//
//  panda.h
//  
//
//  Created by Ahsan Saba on 02/08/2017.
//
//

#ifndef panda_h
#define panda_h

#include <stdio.h>

#endif /* panda_h */

/* Download */
struct panda_state
{
    uint64_t bitrates[MAX_SUPPORTED_BITRATE_LEVELS];
    uint64_t segment_duration;
    uint64_t	    target_inter_request_time; /* t_hat of PANDA (musec) */
    uint64_t    target_bitrate; /* x_hat of PANDA (bits per sec) $$Average probed bitrate*/
    uint64_t    target_avg_bitrate; /* y_hat of PANDA (bits per sec) */
    uint64_t    rate_limit; /* rate cap for sending current segment */
    int         segment;    /* current segment for downloading */
    vlc_mutex_t lock_wait;  /* protect segment download counter */
    vlc_cond_t  wait;       /* some condition to wait on */
} ;


int BandwidthAdaptation(long tput, struct panda_state * panda,
                        long download_duration,
                        long *target_inter_request_time,
                        long *target_bitrate, long buffer_duration,
                        long *target_avg_bitrate, long *rate_limit,
                        int prev_stream);

int getMaxBitrateBelowBandwidth(struct panda_state  * panda, long throughput);
