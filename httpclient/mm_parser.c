#include "mm_parser.h"
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

//#define DEBUG
//#define DECODE

#define DIV_ROUND_CLOSEST(x, divisor)(                  \
{                                                       \
        typeof(divisor) __divisor = divisor;            \
        (((x) + ((__divisor) / 2)) / (__divisor));      \
}                                                       \
)


#define SEC2PICO UINT64_C(1000000000000)
//#define SEC2NANO 1000000000
#define SEC2MILI 1000
static uint minbuffer = MIN_PREBUFFER;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;


static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
static int audio_frame_count = 0;

static unsigned long long atb = 0;
static unsigned long long vtb = 0;

static int decode_packet(int *got_frame, int cached, struct metrics * m)
{
    int ret = 0;
    int decoded = pkt.size;
    
    *got_frame = 0;
    
    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }
        
        if (*got_frame) {
            
            if (frame->width != width || frame->height != height ||
                frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name(frame->format));
                return -1;
            }
            if(pkt.dts > 0)
            {
                m->TSlist[STREAM_VIDEO] = (pkt.dts * vtb) / (SEC2PICO / SEC2MILI);
            //     printf("TS now: %llu, frame type: %d\r",  m->TSlist[STREAM_VIDEO], pkt.flags&AV_PKT_FLAG_KEY?1:0); fflush(stdout);
            /*SA-10214- checkstall should be called after the TS is updated for each stream, instead of when new packets
             arrive, this ensures that we know exactly what time the playout would stop and stall would occur*/
                checkstall(0, m);
            
           // if(video_frame_count)
               /* printf("video_frame%s n:%d coded_n:%d ts:%lld\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number, cached ? (pkt.pts * vtb) / (SEC2PICO / SEC2MILI) : (pkt.dts * vtb) / (SEC2PICO / SEC2MILI));*/
            }
            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          pix_fmt, width, height);
            
        }
    } else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);
        
        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

        }
    }
    
    
    return decoded;
}



static int mm_read(void * opaque, uint8_t *buf, int buf_size)
{
    transport * t =  ((transport *) opaque);
    int ret ;
    
    pthread_mutex_lock(&t->msg_mutex);
    if (t->init_segment_downloaded == 0 )
    {
        pthread_cond_wait( &t->init_ready, &t->msg_mutex );
        t->init_segment_downloaded = 1;
    }


    if (is_empty(t->rx_buf))
    {
        if ( t->stream_complete == 1)
        {
            pthread_mutex_unlock(&t->msg_mutex);
            return 0;
        }
        else
        {
            pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
        }
    }
    
    do{
        ret = pop_message (t->rx_buf, buf, buf_size);
        ++t->rx_buf->lost_packets;
    }while (ret==0);

    if(ret<0)
        return 0;
    else if (ret <= buf_size)
    {
        pthread_cond_signal(&t->msg_ready);
    }
    
    pthread_mutex_unlock(&t->msg_mutex);
    
    return ret;
    
}



static int open_codec_context(int *stream_idx,
                              AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        
        /* find decoder for the stream */
        dec_ctx = st->codec;
        dec = avcodec_find_decoder(dec_ctx->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        
        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", "0", 0);
        if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    
    return 0;
}


static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;
    
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
    
    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}


void * mm_parser(void * opaque)
{
    int got_frame;
    struct metrics * m = (struct metrics * ) opaque;
    minbuffer = m->minbufferlen; 
    av_register_all();
	void *buff = av_malloc(HOLLYWOOD_MSG_SIZE);
	AVIOContext *avio = avio_alloc_context(buff, HOLLYWOOD_MSG_SIZE, 0,
			m->t, mm_read, NULL, NULL);
	if (avio == NULL) {
        fprintf(stderr, "Could not alloc avio context\n\n");
        return NULL;
	}

	AVFormatContext *fmt_ctx = avformat_alloc_context();
	if (fmt_ctx == NULL) {
        fprintf(stderr, "Could not alloc fmt context\n\n");
        return NULL;
    }
	fmt_ctx->pb = avio;

	int ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
	if (ret < 0) {
        char errbuf[256];
        av_strerror	(ret, errbuf, 256);
        fprintf(stderr, "Could not open source file %d\n%s\n", ret, errbuf);
        return NULL;
	}

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return NULL;
    }

    if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dec_ctx = video_stream->codec;
        
        
        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data, video_dst_linesize,
                             width, height, pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto end;
        }
        video_dst_bufsize = ret;
    }
    
    if (open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        audio_dec_ctx = audio_stream->codec;
    }
    
    /* dump input information to stderr */
    //av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    
#ifdef DECODE
    if (video_stream_idx != -1) {
        int vnum = fmt_ctx->streams[video_stream_idx]->time_base.num;
        if (vnum > (int) (UINT64_MAX / SEC2PICO)) {
            exit(EXIT_FAILURE);
        }
        int vden = fmt_ctx->streams[video_stream_idx]->time_base.den;
        vtb = DIV_ROUND_CLOSEST(vnum * SEC2PICO, vden);
    }
    
    if (audio_stream_idx != -1) {
        int anum = fmt_ctx->streams[audio_stream_idx]->time_base.num;
        if (anum > (int) (UINT64_MAX / SEC2PICO)) {
            exit(EXIT_FAILURE);
        }
        int aden = fmt_ctx->streams[audio_stream_idx]->time_base.den;
        atb = DIV_ROUND_CLOSEST(anum * SEC2PICO, aden);
    }
    
    
    if (!audio_stream && !video_stream) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        ret = 1;
        goto end;
    }
    
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        do {
            ret = decode_packet(&got_frame, 0, m);
            if (ret < 0)
                break;
            pkt.data += ret;
            pkt.size -= ret;
        } while (pkt.size > 0);
        av_packet_unref(&orig_pkt);
    }
    
    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;
    do {
        decode_packet(&got_frame, 1, m);
    } while (got_frame);
    
    
    if (audio_stream) {
        enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
        int n_channels = audio_dec_ctx->channels;
        const char *fmt;
        
        if (av_sample_fmt_is_planar(sfmt)) {
            const char *packed = av_get_sample_fmt_name(sfmt);
            printf("Warning: the sample format the decoder produced is planar "
                   "(%s). This example will output the first channel only.\n",
                   packed ? packed : "?");
            sfmt = av_get_packed_sample_fmt(sfmt);
            n_channels = 1;
        }
        
        if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
            goto end;
        
    }
    
    
#else 
    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    unsigned int i;
    for (i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
        } else if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
        }
    }
    
    unsigned long long vtb = 0;
    if (videoStreamIdx != -1) {
        int vnum = fmt_ctx->streams[videoStreamIdx]->time_base.num;
        if (vnum > (int) (UINT64_MAX / SEC2PICO)) {
            fprintf(stderr, "vnum exceeds max uint64 value\n");
            goto end;
        }
        int vden = fmt_ctx->streams[videoStreamIdx]->time_base.den;
        vtb = DIV_ROUND_CLOSEST(vnum * SEC2PICO, vden);
    }
    
    unsigned long long atb = 0;
    if (audioStreamIdx != -1) {
        int anum = fmt_ctx->streams[audioStreamIdx]->time_base.num;
        if (anum > (int) (UINT64_MAX / SEC2PICO)) {
            fprintf(stderr, "anum exceeds max uint64 value\n");
            goto end;
        }
        int aden = fmt_ctx->streams[audioStreamIdx]->time_base.den;
        atb = DIV_ROUND_CLOSEST(anum * SEC2PICO, aden);
    }
    
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == videoStreamIdx) {
            if (pkt.dts > 0) {
                m->TSlist[STREAM_VIDEO] = (pkt.dts * vtb) / (SEC2PICO / SEC2MILI);
            //    printf("TS now: %llu, frame type: %d\n",  m->TSlist[STREAM_VIDEO], pkt.flags&AV_PKT_FLAG_KEY?1:0); fflush(stdout);
                /*SA-10214- checkstall should be called after the TS is updated for each stream, instead of when new packets
                 arrive, this ensures that we know exactly what time the playout would stop and stall would occur*/
                checkstall(0, m);
            }
        } else if (pkt.stream_index == audioStreamIdx) {
            if (pkt.dts > 0) {
                m->TSlist[STREAM_VIDEO] = (pkt.dts * atb) / (SEC2PICO / SEC2MILI);
                /*SA-10214- checkstall should be called after the TS is updated for each stream, instead of when new packets
                 arrive, this ensures that we know exactly what time the playout would stop and stall would occur*/
                checkstall(0, m);
            }
        }
        av_packet_unref(&pkt);

    }
#endif
    
end:
    avcodec_close(video_dec_ctx);
    avcodec_close(audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
    
    m->etime = gettimelong();
	return m;
}


int init_metrics(struct metrics *metric)
{
    memzero(metric, sizeof(*metric));
    metric->t                       =NULL;
    metric->Tplay                    =-1;
    metric->T0                      = -1;
    metric->htime                   = gettimelong();
    metric->Tempty                   = -1;
    metric->initialprebuftime       = -1;
    return 0;
}

int stall_imminent(struct metrics * metric)
{
    long long timenow = gettimelong();
    /* check if there is less than 50ms of video left in buffer. Tplay is the start of playout.
     * if Tplay is -1 this playout has not started so no need to check
    */
    if(metric->Tplay >= 0 && ((double)(metric->TSnow-metric->TS0)*1000)-(timenow-metric->Tplay)<=50000)
    {
        return 1;
    }
    else
        return 0;

}


/* If end is true, it means video has finished downloading. The function will only be called at that time to
 * un-pause in case it is stalled and prebuffering.
 * This function controls the playout and measures the stalls. It is called from inside the savetag functions
 * whenever there is a new TS. The savetag function only needs to update metric->TSnow before calling this function.
 */
void checkstall(int end, struct metrics * m)
{
    long long timenow = gettimelong();
    /*If more than one stream is being downloaded, they need to be synchronized
     metric->TSnow should be the smallest TS in the list, since it represents the highest TS that can be played out.*/
    /*SA 18.11.2014: NUMOFSTREAMS set to 2 for AUDIO and VIDEO. Since mm_parser stores timestamps to TSlist even when both streams are
     in same file, so the check for metric->numofstreams shouldn't be done.*/
    //	if(metric->numofstreams > 1)
    //	{
    m->TSnow= m->TSlist[0];
    int i;
    for(i=1; i<NUMOFSTREAMS; i++)
    {
        if(m->TSnow>m->TSlist[i])
            m->TSnow=m->TSlist[i];
    }
    //	}
    if(m->T0 < 0)
    {
        /*initial run, initialize values*/
        m->T0       = timenow; /* arrival time of first packet*/
        m->Tempty   = timenow; /*time at which prebuffering started*/
        m->TS0      = m->TSnow; /*the earliest timestamp to be played out after prebuffering*/
    }
    /* check if there is a stall, reset time values if there is. Tplay is the start of playout.
     * if Tplay is -1 this playout has not started so no need to check
     * Adding 10ms (10000us) for the encoder and whatnots delay*/
    if(m->Tplay >= 0)
    {
        long time_to_decode = m->Tplay + (double)((m->TSnow - m->TS0)*1000);
        //printf("Time to decode : %lld, timenow %lld, time to wait %lld\n", time_to_decode, timenow, time_to_decode -timenow ); fflush(stdout);
        if ( time_to_decode < timenow)
        {
            m->Tempty = m->Tplay + ((m->TSnow - m->TS0) * 1000);
            m->Tplay = -1;
            m->TS0 = m->TSnow;
#ifdef DEBUG
            printf("Stall has occured at TS: %" PRIu64 " and Time: %lld\n", m->TSnow, m->Tempty); //calculate stall duration
#endif
        }
#ifndef DECODE
        else
        {
            long time_to_wait = time_to_decode - timenow - 10000;
            pthread_mutex_lock(&m->t->msg_mutex);
            if ( m->t->stream_complete == 0 && time_to_wait > 0)
            {
                pthread_mutex_unlock(&m->t->msg_mutex);
                usleep(time_to_wait);
            }
            else
            {
                pthread_mutex_unlock(&m->t->msg_mutex);
            }
        }
#endif
        pthread_mutex_lock(&m->t->msg_mutex);
        m->t->playout_time = m->TSnow;
        pthread_mutex_unlock(&m->t->msg_mutex);
        
    }
    
    /*if Tplay<0, then video is buffering; check if prebuffer is reached*/
    if(m->Tplay < 0)
    {
        if(m->TSnow - m->TS0 >= minbuffer || end)
        {
            m->Tplay = timenow;
#ifdef DEBUG
            printf("Min prebuffer has occured at TS: %" PRIu64 "and Time: %" PRIu64 ", start time %lld \n", m->TSnow, timenow, m->T0);
            
#endif
            if(m->initialprebuftime<0)
            {
                m->initialprebuftime = (double)(m->Tplay - m->stime);
                m->startup = (double)(m->Tplay - m->htime);
                /*stalls need shorter prebuffering, so change minbufer now that initial prebuf is done. */
                minbuffer = m->minbufferlen/2;
            }
            else
            {
                printf("youtubeevent12;%ld;%ld;%" PRIu64 ";%.3f\n",(long)gettimeshort(),(long)m->htime/1000000, m->TS0, (double)(m->Tplay - m->Tempty)/1000);
                m->numofstalls++;
                m->totalstalltime+=(double)(m->Tplay - m->Tempty);
            }
        }
#ifdef DEBUG
        else
            printf("Prebuffered time %" PRIu64 ", TSnow %" PRIu64 ", TS0 %" PRIu64 "\n", m->TSnow - m->TS0, m->TSnow, m->TS0); fflush(stdout);
#endif
        pthread_mutex_lock(&m->t->msg_mutex);
        m->t->playout_time = m->TS0;
        pthread_mutex_unlock(&m->t->msg_mutex);
        
    }
    
}


void printmetric(struct metrics metric, transport media_transport )
{    
    printf("ALL.1;");
    printf("%lld;",         (metric.etime-metric.stime)/1000);   //download time
    printf("%ld;",          metric.minbufferlen); 
    printf("%"PRIu64";",    metric.TSnow); // duration
    printf("%.0f;",         metric.startup/1000); /*startup delay*/
    printf("%.0f;",         metric.initialprebuftime/1000); // Initial prebuf time

    printf("%d;",           metric.numofstalls); //num of stalls
    printf("%.0f;",         (metric.numofstalls>0 ? (metric.totalstalltime/metric.numofstalls/1000) : 0)); // av stall duration
    printf("%.0f;",         metric.totalstalltime/1000); // total stall time
    printf("%lld;",         media_transport.rx_buf->total_bytes_received);
    printf("%lld;",         media_transport.rx_buf->total_bytes_pushed);
    printf("%d\n",         media_transport.rx_buf->late_or_duplicate_packets);
    printf("%d\n",         media_transport.rx_buf->lost_packets);

    fflush(stdout); 
    
    
}


