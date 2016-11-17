#include "mm_parser.h"
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

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


static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    static uint32_t offset = 0;
	struct metrics * m = ((struct metrics *) opaque);
    int read_len;
    uint8_t substream_id;

    if(m->Hollywood)
    {
        /* Receive message loop */
        read_len = recv_message(&(m->h_sock), buf, buf_size, 0, &substream_id);
        if(read_len<0)
            return read_len;
        read_len-=4;
        uint32_t tmp;
        memcpy(&tmp,buf+read_len, sizeof(uint32_t));
        offset+=ntohl(tmp);
        printf("HOLLYWOOD: %d : %u : %u\n", read_len, ntohl(tmp), tmp);
    }
    else
    {
        read_len = recv(m->sock, buf, buf_size, 0);
    }

	return read_len;
}

void mm_parser(struct metrics * m) {
    av_register_all();
	void *buff = av_malloc(MAX_BUFFER_SIZE);
	AVIOContext *avio = avio_alloc_context(buff, MAX_BUFFER_SIZE, 0,
			m, read_packet, NULL, NULL);
	if (avio == NULL) {
		exit(EXIT_FAILURE);
	}

	AVFormatContext *fmt_ctx = avformat_alloc_context();
	if (fmt_ctx == NULL) {
		exit(EXIT_FAILURE);
	}
	fmt_ctx->pb = avio;

	int ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
	if (ret < 0) {
        char errbuf[256];
        av_strerror	(ret,errbuf,256);
        fprintf(stderr, "Could not open source file %d\n%s\n", ret, errbuf);

        exit(EXIT_FAILURE);
	}

	avformat_find_stream_info(fmt_ctx, NULL);

	int videoStreamIdx = -1;
	int audioStreamIdx = -1;
	for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
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
			exit(EXIT_FAILURE);
		}
		int vden = fmt_ctx->streams[videoStreamIdx]->time_base.den;
		vtb = DIV_ROUND_CLOSEST(vnum * SEC2PICO, vden);
	}

	unsigned long long atb = 0;
	if (audioStreamIdx != -1) {
		int anum = fmt_ctx->streams[audioStreamIdx]->time_base.num;
		if (anum > (int) (UINT64_MAX / SEC2PICO)) {
			exit(EXIT_FAILURE);
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
                 printf("Current video timestamp : %llu\n",(pkt.dts * vtb) / (SEC2PICO / SEC2MILI));
				/*SA-10214- checkstall should be called after the TS is updated for each stream, instead of when new packets 
				  arrive, this ensures that we know exactly what time the playout would stop and stall would occur
				checkstall(false); */
			}
		} else if (pkt.stream_index == audioStreamIdx) {
			if (pkt.dts > 0) {
				printf("Current audio timestamp : %llu\n",(pkt.dts * atb) / (SEC2PICO / SEC2MILI));
				/*SA-10214- checkstall should be called after the TS is updated for each stream, instead of when new packets 
				  arrive, this ensures that we know exactly what time the playout would stop and stall would occur
				checkstall(false); */
			}
		}
		av_packet_unref(&pkt);
	}

	avformat_free_context(fmt_ctx);
	av_free(avio);

	return;
}
