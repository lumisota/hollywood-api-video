#include "mm_parser.h"
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define DEBUG
#define DECODE

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
static const char *video_dst_filename = "video_out";
static const char *audio_dst_filename = "audio_out";
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

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

static int decode_packet(int *got_frame, int cached)
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
            
            if(video_frame_count%1000==0)
                printf("video_frame%s n:%d coded_n:%d ts:%lld\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number, cached ? (pkt.pts * vtb) / (SEC2PICO / SEC2MILI) : (pkt.dts * vtb) / (SEC2PICO / SEC2MILI));
            
            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          pix_fmt, width, height);
            
            /* write to rawvideo file */
          //  fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
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
            
            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */
            fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
        }
    }
    
    
    return decoded;
}



static int mm_read(void * opaque, uint8_t *buf, int buf_size)
{
    transport * t = ((transport *) opaque);
    int ret ;
    
    pthread_mutex_lock(&t->msg_mutex);

    if ( t->rx_buf == NULL)
    {
        if ( t->stream_complete == 1)
        {
            printf("Parser thread exiting with 0\n");
            pthread_mutex_unlock(&t->msg_mutex);
            return 0;
        }
        else
            pthread_cond_wait( &t->msg_ready, &t->msg_mutex );
    }
    
    if ( t->buf_len > buf_size )
    {
        ret = buf_size;
    }
    else
    {
        ret = t->buf_len;
    }
  
    t->buf_len -= ret;

    
    memcpy(buf, t->rx_buf, ret);
    
    if ( t->buf_len == 0 )
    {
        t->rx_buf = NULL;
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
    av_register_all();
	void *buff = av_malloc(100*1024);
	AVIOContext *avio = avio_alloc_context(buff, MAX_BUFFER_SIZE, 0,
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
        
        video_dst_file = fopen(video_dst_filename, "wb");
        if (!video_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
            ret = 1;
            goto end;
        }
        
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
        audio_dst_file = fopen(audio_dst_filename, "wb");
        if (!audio_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", audio_dst_filename);
            ret = 1;
            goto end;
        }
    }
    
    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    
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
    
    if (video_stream)
        printf("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
    if (audio_stream)
        printf("Demuxing audio from file '%s' into '%s'\n", src_filename, audio_dst_filename);

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        do {
            ret = decode_packet(&got_frame, 0);
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
        decode_packet(&got_frame, 1);
    } while (got_frame);
    
    printf("Demuxing succeeded.\n");
    
    if (video_stream) {
        printf("Play the output video file with the command:\n"
               "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
               av_get_pix_fmt_name(pix_fmt), width, height,
               video_dst_filename);
    }
    
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
        
        printf("Play the output audio file with the command:\n"
               "ffplay -f %s -ac %d -ar %d %s\n",
               fmt, n_channels, audio_dec_ctx->sample_rate,
               audio_dst_filename);
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
                printf("TS now: %llu, frame type: %d\n",  m->TSlist[STREAM_VIDEO], pkt.flags&AV_PKT_FLAG_KEY?1:0);
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
    if (video_dst_file)
        fclose(video_dst_file);
    if (audio_dst_file)
        fclose(audio_dst_file);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
    
    m->etime = gettimelong();
	return m;
}


int init_metrics(struct metrics *metric)
{
    memzero(metric, sizeof(*metric));
    metric->t                       =NULL;
    metric->Tmin                    =-1;
    metric->T0                      = -1;
    metric->htime                   = gettimelong();
    metric->Tmin0                   = -1;
    metric->initialprebuftime       = -1;
    /*if this value is set to 0, the whole file is requested. */
    metric->playout_buffer_seconds  =0;
    return 0;
}

int stall_imminent(struct metrics * metric)
{
    long long timenow = gettimelong();
    /* check if there is less than 50ms of video left in buffer. Tmin is the start of playout.
     * if Tmin is -1 this playout has not started so no need to check
    */
    if(metric->Tmin >= 0 && ((double)(metric->TSnow-metric->TS0)*1000)-(timenow-metric->Tmin)<=50000)
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
void checkstall(int end, struct metrics * metric)
{
    long long timenow = gettimelong();
    /*If more than one stream is being downloaded, they need to be synchronized
     metric->TSnow should be the smallest TS in the list, since it represents the highest TS that can be played out.*/
    /*SA 18.11.2014: NUMOFSTREAMS set to 2 for AUDIO and VIDEO. Since mm_parser stores timestamps to TSlist even when both streams are
     in same file, so the check for metric->numofstreams shouldn't be done.*/
    //	if(metric->numofstreams > 1)
    //	{
    metric->TSnow= metric->TSlist[0];
    int i;
    for(i=1; i<NUMOFSTREAMS; i++)
    {
        if(metric->TSnow>metric->TSlist[i])
            metric->TSnow=metric->TSlist[i];
    }
    //	}
    if(metric->T0 < 0)
    {
        /*initial run, initialize values*/
        metric->T0 = timenow; /* arrival time of first packet*/
        metric->Tmin0= timenow; /*time at which prebuffering started*/
        metric->TS0 = metric->TSnow; /*the earliest timestamp to be played out after prebuffering*/
    }
    /* check if there is a stall, reset time values if there is. Tmin is the start of playout.
     * if Tmin is -1 this playout has not started so no need to check
     * Adding 10ms (10000us) for the encoder and whatnots delay*/
    if(metric->Tmin >= 0 && ((double)(metric->TSnow-metric->TS0)*1000) <=(timenow-metric->Tmin))
    {
        metric->Tmin0=metric->Tmin+((metric->TSnow-metric->TS0)*1000);
        metric->Tmin = -1;
        metric->TS0 = metric->TSnow;
#ifdef DEBUG
        printf("Stall has occured at TS: %" PRIu64 " and Time: %lld\n", metric->TSnow, metric->Tmin0); //calculate stall duration
#endif
        
    }
    
    /*if Tmin<0, then video is buffering; check if prebuffer is reached*/
    if(metric->Tmin< 0)
    {
        if(metric->TSnow-metric->TS0 >= minbuffer || end)
        {
            metric->Tmin = timenow;
#ifdef DEBUG
            printf("Min prebuffer has occured at TS: %" PRIu64 "and Time: %" PRIu64 ", start time %lld \n", metric->TSnow, timenow, metric->T0);
            
#endif
            if(metric->initialprebuftime<0)
            {
                metric->initialprebuftime=(double)(metric->Tmin-metric->stime);
                metric->startup =(double)(metric->Tmin-metric->htime);
                /*stalls need shorter prebuffering, so change minbufer now that initial prebuf is done. */
                minbuffer = MIN_STALLBUFFER;
            }
            else
            {
                printf("youtubeevent12;%ld;%ld;%" PRIu64 ";%.3f\n",(long)gettimeshort(),(long)metric->htime/1000000, metric->TS0, (double)(metric->Tmin-metric->Tmin0)/1000);
                ++metric->numofstalls;
                metric->totalstalltime+=(double)(metric->Tmin-metric->Tmin0);
            }
        }
#ifdef DEBUG
        else
            printf("Prebuffered time %" PRIu64 ", TSnow %" PRIu64 ", TS0 %" PRIu64 "\n",metric->TSnow-metric->TS0, metric->TSnow, metric->TS0); fflush(stdout);
#endif
        
    }
    
}

void printmetric(struct metrics metric)
{    
    printf("ALL.1;");
    printf("%lld;",         metric.etime-metric.stime);   //download time
    printf("%"PRIu64";",    metric.TSnow*1000); // duration
    printf("%.0f;",         metric.startup); /*startup delay*/
    printf("%.0f;",         metric.initialprebuftime); // Initial prebuf time
           
   // printf("VIDEO.1;");
    printf("%.0f;",         metric.totalbytes[STREAM_VIDEO]); //total bytes
    
    printf("%d;",           metric.numofstalls); //num of stalls
    printf("%.0f;",         (metric.numofstalls>0 ? (metric.totalstalltime/metric.numofstalls) : 0)); // av stall duration
    printf("%.0f;",         metric.totalstalltime); // total stall time

    printf("%d;\n",         metric.playout_buffer_seconds); /*range*/
    
    
}


