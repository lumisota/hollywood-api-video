//
//  bola.c
//  
//
//  Created by Saba Ahsan on 24/03/17.
//
//

#include "bola.h"
//#define BOLA ""
//#define BOLAMETRIC ""

#define BOLAMETRIC "ABR"
#define BOLA "BOLA"


float getStableBufferTime()
{
    return DEFAULT_MIN_BUFFER_TIME;
}


void saveThroughput (struct bola_state * bola , long curr_throughput)
{
    static int index = 0;
    if (curr_throughput < 0) 
        return; 
    printf("Saving throughput %lld\n", curr_throughput); 
    bola->throughput[index] = curr_throughput;
    
    ++index;
    
    if(index >= AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE)
        index = 0;
    
    return;
}

long getRecentThroughput (struct bola_state * bola)
{
    long total      = 0;
    int samples     = 0;
    
    for ( int i = 0; i < AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE; i++)
    {
        if (bola->throughput[i] >= 0)
        {
            total+=bola->throughput[i];
            samples+=1;
        }
    }
    printf("Extracted throughput from %d samples: %ld\n", samples, total); 
    if (samples>0)
        return (total/samples);
    else
        return 0;
}

int getQualityFromThroughput(struct bola_state * bola, long throughput )
{
    // do not factor in bandwidthSafetyFactor here - it is factored at point of function invocation
    int q = 0;
    
    for ( int i = 0; i < bola->num_of_levels; i++)
    {
        if (bola->bitrates[i] < throughput)
            q = i;
        else
            break;
    }
    
    return q;
}

int getQualityFromBufferLevel(struct bola_state * bola, float bufferLevel)
{
    long quality    = -1;
    float score, s;
    
    for ( int i = 0; i < bola->num_of_levels; i++)
    {
        s = (bola->Vp * (bola->utilities[i] + bola->gp) - bufferLevel) / bola->bitrates[i];
        if (quality < 0 || s >= score) {
            score = s;
            quality = i;
        }
    }
    return quality;
}


// NOTE: in live streaming, the real buffer level can drop below minimumBufferS, but bola should not stick to lowest bitrate by using a placeholder buffer level
int calculateParameters(int minimumBufferS, int bufferTargetS, manifest * m, struct bola_state * bola)
{
    int highestUtilityIndex = -1;
    int i;
    
    for ( i = 0 ; i < m->num_of_levels; i++)
    {
        bola->bitrates[i] = (double)m->bitrate_level[i].bitrate;
        bola->utilities[i] = log ((float)bola->bitrates[i]/bola->bitrates[0]);
        pprintdebug(BOLA, "Level = %d\tBitrate = %f\tUtility = %f", i, bola->bitrates[i], bola->utilities[i]);

    }
    bola->num_of_levels = m->num_of_levels;
    highestUtilityIndex = i - 1;
    
    if (highestUtilityIndex <= 0) {
        // if highestUtilityIndex === 0, then always use lowest bitrate
        return 0;
    }
    
    
    
    // TODO: Investigate if following can be better if utilities are not the default Math.log utilities.
    // If using Math.log utilities, we can choose Vp and gp to always prefer bitrates[0] at minimumBufferS and bitrates[max] at bufferTargetS.
    // (Vp * (utility + gp) - bufferLevel) / bitrate has the maxima described when:
    // Vp * (utilities[0] + gp - 1) = minimumBufferS and Vp * (utilities[max] + gp - 1) = bufferTargetS
    // giving:
    bola->gp = 1 - bola->utilities[0] + (bola->utilities[highestUtilityIndex] - bola->utilities[0]) / (bufferTargetS / minimumBufferS - 1);
    bola->Vp = minimumBufferS / (bola->utilities[0] + bola->gp - 1);
    bola->highestUtilityIndex = highestUtilityIndex;
    
    pprintdebug(BOLA, "Initialized gamma = %d, V = %d, %d", bola->gp, bola->Vp, highestUtilityIndex);
    
    return highestUtilityIndex;
}


int calculateInitialState(manifest * m, int isDynamic, struct bola_state * initialState)
{
    if (calculateParameters(MINIMUM_BUFFER_S, BUFFER_TARGET_S, m, initialState) == 0) {
        // The best soloution is to always use the lowest bitrate...
        initialState->state = BOLA_STATE_ONE_BITRATE;
        pprintdebug(BOLA, "Initialized to ONE BITRATE STATE");
        return 0;
    }
    
    initialState->state                 = BOLA_STATE_STARTUP;
    initialState->isDynamic             = isDynamic;
    initialState->fragmentDuration      = m->segment_dur_ms;
    
    /* SA: COMMENTS COPIED FROM streaming/MediaPlayer.js, we hardcode the default value
     * A percentage between 0.0 and 1 to reduce the measured throughput calculations.
     * The default is 0.9. The lower the value the more conservative and restricted the
     * measured throughput calculations will be. please use carefully. This will directly
     * affect the ABR logic in dash.js*/
    
    initialState->bandwidthSafetyFactor = 0.85;
    initialState->rebufferSafetyFactor  = REBUFFER_SAFETY_FACTOR;
    initialState->bufferTarget          = getStableBufferTime();
    
    initialState->lastQuality           = 0;
    initialState->placeholderBuffer     = 0;
    
    for (int i = 0; i < AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE; i++)
    {
        initialState->throughput[i] = -1; 
    }
    
//    if (BOLA_DEBUG) {
//        let info = '';
//        for (let i = 0; i < bitrates.length; ++i) {
//            let u  = params.utilities[i];
//            let b  = bitrates[i];
//            let th = 0;
//            if (i > 0) {
//                let u1 = params.utilities[i - 1];
//                let b1 = bitrates[i - 1];
//                th  = params.Vp * ((u1 * b - u * b1) / (b - b1) + params.gp);
//            }
//            let z = params.Vp * (u + params.gp);
//            info += '\n' + i + ':' + (0.000001 * bitrates[i]).toFixed(3) + 'Mbps ' + th.toFixed(3) + '/' + z.toFixed(3);
//        }
//        log('BolaDebug ' + mediaInfo.type + ' bitrates' + info);
//    }
    pprintdebug(BOLA, "Initialized to STATE STARTUP");

    return 1;
}



int getFirstIndex(struct bola_state * bola)
{
    if (bola->state != BOLA_STATE_ONE_BITRATE) {
        // Bola is not invoked by dash.js to determine the bitrate quality for the first fragment. We might estimate the throughput level here, but the metric related to the HTTP request for the first fragment is usually not available.
        // TODO: at some point, we may want to consider a tweak that redownloads the first fragment at a higher quality
        
        long initThroughput = getRecentThroughput(bola);
        if (initThroughput == 0) {
            // We don't have information about any download yet - let someone else decide quality.
            pprintdebug(BOLA, "Quality initialized to 0, No throughput information");
            return 0;
        }
        int q = getQualityFromThroughput(bola, initThroughput* bola->bandwidthSafetyFactor);
        bola->lastQuality = q;
        pprintdebug(BOLA, "Quality initialized to %d, rule : Throughput %ld", q, initThroughput);
        return q;
    }
    
    return 0;
} // initialization




/*bufferLevel is in seconds*/
int getMaxIndex(struct bola_state * bola, float bufferLevel, long long stime)
{
    double recentThroughput;
    float effectiveBufferLevel;
    int bolaQuality;
    float delaySeconds = 0.0;
    float wantBufferLevel;
    float b;
    float wantEffectiveBuffer = 0;
    int q;


   
    if (bola->state == BOLA_STATE_ONE_BITRATE) {
        pprintdebug(BOLA, "BOLA_STATE_ONE_BITRATE:::Last Quality: 0 , New Quality 0, rule : NONE");
        pprintdebug(BOLAMETRIC, "%lld %.0f, %.0f, NONE, BOLA_STATE_ONE_BITRATE", (gettimelong()-stime)/1000, bola->bitrates[0], bola->bitrates[0]);
        return 0;
    }
    
    recentThroughput = getRecentThroughput(bola);
    
    if (bufferLevel <= 0.1) {
        // rebuffering occurred, reset placeholder buffer
        //SA: I don't understand what's going on!!!
        bola->placeholderBuffer = 0.0;
    }
    
    
    effectiveBufferLevel = bufferLevel + bola->placeholderBuffer;
    
    if (bola->state == BOLA_STATE_STARTUP) {
        // in startup phase, use some throughput estimation
        q = getQualityFromThroughput(bola, recentThroughput* bola->bandwidthSafetyFactor);
        
        if (bufferLevel > bola->fragmentDuration / REBUFFER_SAFETY_FACTOR) {
            // only switch to steady state if we believe we have enough buffer to not trigger quality drop to a safeBitrate
            bola->state = BOLA_STATE_STEADY;
            for (int i = 0; i < q; ++i) {
                // We want minimum effective buffer (bufferLevel + placeholderBuffer) that gives a higher score for q when compared with any other i < q.
                // We want
                //     (Vp * (utilities[q] + gp) - bufferLevel) / bitrates[q]
                // to be >= any score for i < q.
                // We get score equality for q and i when:
                b = bola->Vp * (bola->gp + (bola->bitrates[q] * bola->utilities[i] - bola->bitrates[i] * bola->utilities[q]) / (bola->bitrates[q] - bola->bitrates[i]));
                if (b > wantEffectiveBuffer) {
                    wantEffectiveBuffer = b;
                }
            }
            if (wantEffectiveBuffer > bufferLevel) {
                bola->placeholderBuffer = wantEffectiveBuffer - bufferLevel;
            }
        }
        pprintdebug(BOLA, "BOLA_STATE_STARTUP:::Last Quality: %d , New Quality %d, rule : Throughput (%ld bps)",  bola->lastQuality, q, recentThroughput);
        pprintdebug(BOLAMETRIC, "%lld %.0f %.0f Throughput BOLA_STATE_STARTUP", (gettimelong()-stime)/1000,bola->bitrates[bola->lastQuality], bola->bitrates[q]);

        bola->lastQuality = q;
        return q;
    }
    
    // steady state
    bolaQuality = getQualityFromBufferLevel(bola, effectiveBufferLevel);
    pprintdebug(BOLA, "effectiveBufferLevel %f (%f)",  effectiveBufferLevel, bufferLevel);

    // we want to avoid oscillations
    // We implement the "BOLA-O" variant: when network bandwidth lies between two encoded bitrate levels, stick to the lowest level.
    if (bolaQuality > bola->lastQuality) {
        // do not multiply throughput by bandwidthSafetyFactor here: we are not using throughput estimation but capping bitrate to avoid oscillations
        int q = getQualityFromThroughput(bola, recentThroughput* bola->bandwidthSafetyFactor);
        if (bolaQuality > q) {
            // only intervene if we are trying to *increase* quality to an *unsustainable* level
            
            if (q < bola->lastQuality) {
                // we are only avoid oscillations - do not drop below last quality
                q = bola->lastQuality;
            }
            // We are dropping to an encoding bitrate which is a little less than the network bandwidth because bitrate levels are discrete. Quality q might lead to buffer inflation, so we deflate buffer to the level that q gives postive utility. This delay will be added below.
            bolaQuality = q;
        }
    }
    
    // Try to make sure that we can download a chunk without rebuffering. This is especially important for live streaming.
    if (recentThroughput > 0) {
        // We can only perform this check if we have a throughput estimate.
        uint64_t safeBitrate = REBUFFER_SAFETY_FACTOR * recentThroughput * bufferLevel / bola->fragmentDuration;
        while (bolaQuality > 0 && bola->bitrates[bolaQuality] > safeBitrate) {
            --bolaQuality;
        }
    }
    
    // We do not want to overfill buffer with low quality chunks.
    // Note that there will be no delay if buffer level is below MINIMUM_BUFFER_S, probably even with some margin higher than MINIMUM_BUFFER_S.
    wantBufferLevel = bola->Vp * (bola->utilities[bolaQuality] + bola->gp);
    delaySeconds = effectiveBufferLevel - wantBufferLevel;
    if (delaySeconds > 0)
    {
        // First reduce placeholder buffer.
        // Note that this "delay" is the main mechanism of depleting placeholderBuffer - the real buffer is depleted by playback.
        if (delaySeconds > bola->placeholderBuffer) {
            delaySeconds -= bola->placeholderBuffer;
            bola->placeholderBuffer = 0;
        } else {
            bola->placeholderBuffer -= delaySeconds;
            delaySeconds = 0;
        }
    }
    if (delaySeconds > 0) {
        // After depleting all placeholder buffer, set delay.
        if (bolaQuality == bola->num_of_levels - 1) {
            // At top quality, allow schedule controller to decide how far to fill buffer.
            delaySeconds = 0;
        } else {
            pprintdebug(BOLA, "Delaying to avoid overfilling buffer with low quality chunks (%f sec)", delaySeconds);
            usleep(delaySeconds*1000000);
        }
    } else {
        delaySeconds = 0;
    }
    pprintdebug(BOLA, "BOLA_STATE_STEADY:::Last Quality: %d , New Quality %d, rule : Buffer",  bola->lastQuality, bolaQuality);
    pprintdebug(BOLAMETRIC, "%lld %.0f %.0f Buffer BOLA_STATE_STEADY", (gettimelong()-stime)/1000, bola->bitrates[bola->lastQuality], bola->bitrates[bolaQuality]);

    bola->lastQuality = bolaQuality;
//    metricsModel.updateBolaState(mediaType, bolaState);
//    
//    switchRequest.value = bolaQuality;
//    switchRequest.reason.state = bolaState.state;
//    switchRequest.reason.throughput = recentThroughput;
//    switchRequest.reason.bufferLevel = bufferLevel;
//    
//    if (BOLA_DEBUG) log('BolaDebug ' + mediaType + ' BolaRule quality ' + bolaQuality + ' delay=' + delaySeconds.toFixed(3) + ' for STEADY');

    return bolaQuality;
}



