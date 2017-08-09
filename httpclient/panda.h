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
#include "../common/helper.h"
#include "readmpd.h"


#endif /* panda_h */

/* Download */
struct panda_state
{
    uint64_t bitrates[MAX_SUPPORTED_BITRATE_LEVELS];
    uint64_t segment_duration;
    int num_of_levels; 
} ;

void initialize_panda(struct panda_state * panda, manifest * m);

int BandwidthAdaptation(long tput, struct panda_state * panda,
                        long download_duration,
                        long *target_inter_request_time,
                        long *target_bitrate, long buffer_duration,
                        long *target_avg_bitrate, long *rate_limit,
                        int prev_stream, int panda_enabled);

int getMaxBitrateBelowBandwidth(struct panda_state  * panda, long throughput);
