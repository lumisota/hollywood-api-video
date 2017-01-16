#include "mm_parser.h"
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#define DEBUG

#define MAX_BUFFER_SIZE 100*1024

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


static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    static uint32_t offset = 0;
	struct metrics * m = ((struct metrics *) opaque);
    int read_len;
    uint8_t substream_id;

    if(m->Hollywood)
    {
        /* Receive message loop */
        read_len = recv_message(&(m->h_sock), buf, buf_size, 0, &substream_id);
        if(read_len<=0)
            return read_len;
        read_len-=4;
        uint32_t tmp;
        memcpy(&tmp,buf+read_len, sizeof(uint32_t));
        tmp = ntohl(tmp);
        if(offset<tmp)
        {
            printf("Dropped frame size: %d, offset : %u\n", tmp-offset, offset);
            if(read_len+(tmp-offset)>MAX_BUFFER_SIZE)
                return -1;
            memcpy(buf+(tmp-offset), buf, read_len);
            memzero(buf, tmp-offset);
            read_len+=(tmp-offset);
        }
        offset+=read_len;
        
        /*Write buffer to file for later use*/
        if(fwrite (buf , sizeof(char), read_len, m->fptr)!=read_len)
        {
            if (ferror (m->fptr))
                printf ("Error Writing to file\n");
            perror("The following error occured\n");
            return -1;
        }

    }
    else
    {
        read_len = recv(m->sock, buf, buf_size, 0);
    }

    m->totalbytes[STREAM_VIDEO] += read_len;
	return read_len;
}

int mm_parser(struct metrics * m) {
    av_register_all();
	void *buff = av_malloc(MAX_BUFFER_SIZE);
	AVIOContext *avio = avio_alloc_context(buff, MAX_BUFFER_SIZE, 0,
			m, read_packet, NULL, NULL);
	if (avio == NULL) {
        fprintf(stderr, "Could not alloc avio context\n\n");
        return -1;
	}

	AVFormatContext *fmt_ctx = avformat_alloc_context();
	if (fmt_ctx == NULL) {
        fprintf(stderr, "Could not alloc fmt context\n\n");
        return -1;
    }
	fmt_ctx->pb = avio;

	int ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
	if (ret < 0) {
        char errbuf[256];
        av_strerror	(ret, errbuf, 256);
        fprintf(stderr, "Could not open source file %d\n%s\n", ret, errbuf);
        return -1;
	}

	avformat_find_stream_info(fmt_ctx, NULL);

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
            return -1;
        }
		int vden = fmt_ctx->streams[videoStreamIdx]->time_base.den;
		vtb = DIV_ROUND_CLOSEST(vnum * SEC2PICO, vden);
	}

	unsigned long long atb = 0;
	if (audioStreamIdx != -1) {
		int anum = fmt_ctx->streams[audioStreamIdx]->time_base.num;
		if (anum > (int) (UINT64_MAX / SEC2PICO)) {
            fprintf(stderr, "anum exceeds max uint64 value\n");
            return -1;
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
                printf("TS now: %" PRIu64" \n",  m->TSlist[STREAM_VIDEO]);
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

	avformat_free_context(fmt_ctx);
	av_free(avio);
    m->etime = gettimelong();
    printmetric(*m);
	return 0;
}


void init_metrics(struct metrics *metric)
{
    memzero(metric, sizeof(*metric));
    metric->Tmin=-1;
    metric->T0 = -1;
    metric->htime = gettimelong();
    metric->Tmin0 = -1;
    metric->initialprebuftime = -1;
    /*if this value is set to 0, the whole file is requested. */
    metric->playout_buffer_seconds=0;
    
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
        printf("Stall has occured at TS: %llu and Time: %lld\n", metric->TSnow, metric->Tmin0); //calculate stall duration
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
    double mtotalrate=(metric.totalbytes[STREAM_VIDEO])/(metric.etime-metric.stime);
    
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


