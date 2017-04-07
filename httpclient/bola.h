//
//  bola.h
//  
//
//  Created by Saba Ahsan on 24/03/17.
//
//

#ifndef ____bola__
#define ____bola__

#include <stdio.h>
#include <math.h>
#include "readmpd.h"
#include "mm_parser.h"


#define BOLA_STATE_ONE_BITRATE      0
#define BOLA_STATE_STARTUP          1
#define BOLA_STATE_STEADY           2
#define BOLA_DEBUG                  1
#define MINIMUM_BUFFER_S            10 // BOLA should never add artificial delays if buffer is less than MINIMUM_BUFFER_S.
#define BUFFER_TARGET_S             30 // If Schedule Controller does not allow buffer level to reach BUFFER_TARGET_S, this can be a placeholder buffer level.
#define REBUFFER_SAFETY_FACTOR      0.5 // Used when buffer level is dangerously low, might happen often in live streaming.

#define AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE  3
#define AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_VOD   4

#define DEFAULT_MIN_BUFFER_TIME     12

/* Throughput values are always bps, uint64_t
 Time is in milliseconds,  uint64_t  */

struct bola_state{
    int state;
    double bitrates[MAX_SUPPORTED_BITRATE_LEVELS];
    float utilities[MAX_SUPPORTED_BITRATE_LEVELS];
    int num_of_levels;
    int highestUtilityIndex;
    float Vp;
    float gp;
    
    int isDynamic;
    float fragmentDuration;
    float bandwidthSafetyFactor;
    float rebufferSafetyFactor;
    float bufferTarget;
    
    double throughput[AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE];
    
    int lastQuality;
    float   placeholderBuffer;
    
};


int calculateParameters(int minimumBufferS, int bufferTargetS, manifest * m, struct bola_state * bola);
void saveThroughput (struct bola_state * bola , long curr_throughput);
long getRecentThroughput (struct bola_state * bola);
int getQualityFromThroughput(struct bola_state * bola, long throughput );
int getQualityFromBufferLevel(struct bola_state * bola, float bufferLevel);
int calculateInitialState(manifest * m, int isDynamic, struct bola_state * initialState);
int getFirstIndex(struct bola_state * bola);
int getMaxIndex(struct bola_state * bola, float bufferLevel);


#endif /* defined(____bola__) */


