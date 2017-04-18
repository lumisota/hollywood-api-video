/*
 * Copyright (c)
 *      2012 Stefano Sabatini (FFmpeg example portions)
 *      2017 Saba Ahsan
 *      2017 Stephen McQuistin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "media_sender.h"
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <sys/stat.h>

#define DIV_ROUND_CLOSEST(x, divisor)(                  \
{                                                       \
        typeof(divisor) __divisor = divisor;            \
        (((x) + ((__divisor) / 2)) / (__divisor));      \
}                                                       \
)

#define SEC2PICO UINT64_C(1000000000000)
#define SEC2MILI 1000

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;
int src_filesize;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
int vtb = 0;

pthread_t       av_tid;         /*thread id of the av parser thread*/
pthread_t       h_tid;          /*thread id of the tcp hollywood sender thread*/
pthread_cond_t  msg_ready;      /*indicates whether a full message is ready to be sent*/
pthread_mutex_t msg_mutex;      /*mutex of the hlywd_message*/

extern uint32_t offset;         /*offset added to last 4 bytes of the message*/
extern uint32_t stream_seq;

struct vid_frame {
    int64_t byte_offset;
    int64_t byte_length;
    long unsigned duration;
    int key_frame;
    struct vid_frame *next;
};

struct vid_frame *video_frames = NULL;

/* FFmpeg helper functions */

static int decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;
        
    *got_frame = 0;

    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            return ret;
        }

        if (*got_frame) {
            struct vid_frame *next_frame = (struct vid_frame *) malloc(sizeof(struct vid_frame));
            next_frame->byte_offset = pkt.pos;
            next_frame->next = NULL;
            next_frame->duration = (pkt.dts * vtb) / (SEC2PICO / SEC2MILI);
            next_frame->key_frame = frame->key_frame;
            next_frame->byte_length = src_filesize - next_frame->byte_offset;
            if (video_frames == NULL) {
                video_frames = next_frame;
            } else {
                struct vid_frame *end_frame = video_frames;
                while (end_frame->next != NULL) {
                    end_frame = end_frame->next;
                }
                end_frame->next = next_frame;
                end_frame->byte_length = next_frame->byte_offset-end_frame->byte_offset;
            }
        }
    } 
    return decoded;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
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
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    return 0;
}

/* Add message to the queue of messages*/
int add_msg_to_queue(struct hlywd_message *msg, struct parse_attr *pparams) {
    uint32_t tmp;
    
    /* copy offset to end of message */
    tmp = htonl(offset);
    memcpy(msg->message+msg->msg_size, &tmp, sizeof(uint32_t));
    
    /* copy stream sequence number to end of message */
    tmp = htonl(stream_seq);
    memcpy(msg->message+msg->msg_size+sizeof(uint32_t), &tmp, sizeof(uint32_t));
    
    /* update offset to include this message */    
    offset+=msg->msg_size;
    
    /* update message size to include offset, stream sequence number */
    msg->msg_size+=HLYWD_MSG_TRAILER;

    /* increment stream sequence number */
    stream_seq++;
    
    pthread_mutex_lock(&msg_mutex);
    
    /* if MAXQLEN is reached, wait for it to empty*/
    while (pparams->hlywd_data->qlen>=MAXQLEN) {
        pthread_cond_wait(&msg_ready, &msg_mutex);
    }
    
    /* if the queue is empty, then this is the first entry */
    if (pparams->hlywd_data->hlywd_msg == NULL)
    {
        pparams->hlywd_data->hlywd_msg = msg;
    } else {
        /* otherwise, add this message to the tail of the queue */
        int qlen = 1;
        struct hlywd_message *tmp = pparams->hlywd_data->hlywd_msg;
        while (tmp->next != NULL) {
            tmp = tmp->next;
            qlen++;
        }
        tmp->next = msg;
    }
    
    pparams->hlywd_data->qlen++;
    
    /* if the queue was empty, send signal to wake up any sleeping send thread */
    pthread_cond_signal(&msg_ready);
    pthread_mutex_unlock(&msg_mutex);
    return 1; 
}

/* Fill in Hollywood timing metadata */
void *fill_timing_info(void *pparams_arg) {
    struct parse_attr *pparams = (struct parse_attr *) pparams_arg;
    struct hlywd_message *msg;
    int bytes_read;
    int message_size = HOLLYWOOD_MSG_SIZE - HLYWD_MSG_TRAILER;

    /* 
     * for each frame:
     *  while not all frame bytes read/sent:
     *       read min(remaining bytes, message_size)
     *       queue message for sending
     */
    struct vid_frame *cur_frame = video_frames;
    while (cur_frame != NULL) {
        int64_t remaining_bytes = cur_frame->byte_length;
        fseek(pparams->fptr, cur_frame->byte_offset, SEEK_SET); /* go to start of frame in file */
        while (remaining_bytes > 0) {
            /* initialise an empty Hollywood message */
            msg = (struct hlywd_message *) malloc(sizeof(struct hlywd_message));
            memset(msg, 0, sizeof(struct hlywd_message));
            int64_t to_read = remaining_bytes;
            if (to_read > message_size) {
                to_read = message_size;
            }
            
            bytes_read = fread(msg->message, sizeof(unsigned char), to_read, pparams->fptr);
            
            if (bytes_read != to_read && !feof(pparams->fptr)) {
                /* fewer bytes were read, and EOF not reached: error */
                perror("An error occured while reading the file.\n");
                free(msg);
                return NULL;
            }
            
            /* set Hollywood message metadata */
            msg->msg_size = bytes_read;
            
            /* queue message */
            add_msg_to_queue(msg, pparams);
        
            /* lose pointer; free'd by sender */
            msg = NULL;
            
            remaining_bytes = remaining_bytes - bytes_read;
        }
        cur_frame = cur_frame->next;
    }
    
    pthread_mutex_lock(&msg_mutex);
    /* indicate that file has been completely read */
    pparams->hlywd_data->file_complete = 1;
    /* signal message ready to send */
    pthread_cond_signal(&msg_ready);
    pthread_mutex_unlock(&msg_mutex);
    
    return NULL;
}

/* Write message using Hollywood API */
void *write_to_hollywood(void *hlywd_data_arg) {
    struct hlywd_attr *hlywd_data = (struct hlywd_attr *) hlywd_data_arg;
    struct hlywd_message *msg;
    uint16_t depends_on;
    int msg_len;

    /* if not sending, open file to save to instead */
#ifdef NOSEND
    char filename[256] = "saved_output2.mp4";
    FILE *fptr;
    printf("Saving to file: %s\n", filename);
    fflush(stdout);
    fptr = fopen(filename,"wb");
    if (fptr == NULL) {
        perror("Error opening file:");
        return NULL;
    }
#endif
    
    while(1) {
        pthread_mutex_lock(&msg_mutex);
        
        /* if queue is empty and EOF not reached, wait for more messages */
        while (hlywd_data->qlen == 0 && !hlywd_data->file_complete)
        {
            pthread_cond_wait(&msg_ready, &msg_mutex);
        }
        
        /* if queue is empty and EOF has been reached, stop */
        if (hlywd_data->qlen == 0 && hlywd_data->file_complete) {
            pthread_mutex_unlock(&msg_mutex);
            break;
        }
        
        /* get message at top of queue */
        msg = hlywd_data->hlywd_msg;
        
        /* check message isn't empty */
        if (msg == NULL) {
            printf("Error: Empty message was found in the sender queue\n");
            break;
        }
        
        /* dequeue message at top of queue */
        hlywd_data->hlywd_msg = hlywd_data->hlywd_msg->next;
        hlywd_data->qlen--;
        
        /* if the queue was full, send a signal to wake parser thread */
        if (hlywd_data->qlen <= MAXQLEN) {
            pthread_cond_signal(&msg_ready);
        }
        
        /* if not sending, write message to file */
#ifdef NOSEND
        if (fwrite(msg->message,sizeof(unsigned char), msg->msg_size, fptr)!=msg->msg_size) {
            if (ferror(fptr)) {
                printf("Error Writing to %s\n", filename);
            }
            perror("The following error occured\n");
            free(msg);
            pthread_mutex_unlock(&msg_mutex);
            break;
        }     
#else
        /* otherwise, use Hollywood API to send the message */
        if (msg->depends_on != 0) {
            depends_on = hlywd_data->seq + msg->depends_on;
        } else {
            depends_on = 0;
        }
        msg_len = send_message_time(hlywd_data->hlywd_socket, msg->message, msg->msg_size, 0, hlywd_data->seq, depends_on, msg->lifetime_ms);
        fflush(stdout);
        hlywd_data->seq++;
        if (msg_len == -1) {
            printf("Unable to send message over Hollywood\n");
            free(msg);
            pthread_mutex_unlock(&msg_mutex);
            break; 
        }
#endif
        free(msg);
        pthread_mutex_unlock(&msg_mutex);
        
    }
    return NULL;  
}

/* Send media file using Hollywood */
int send_media_over_hollywood(hlywd_sock * sock, FILE *fptr, int seq, char *src_filename) {
    struct hlywd_attr hlywd_data = {0};
    struct parse_attr pparams = {0};
    pthread_attr_t attr;

    /* initialise attribute structures */
    pparams.fptr = fptr; /*file pointer for reading mp4 file*/
    pparams.hlywd_data = &hlywd_data; /* Hollywood metadata structure */
    hlywd_data.hlywd_socket = sock; /* Hollywood socket */
    
    /* if sending, set playout delay via Hollywood API */
#ifndef NOSEND 
    set_playout_delay(hlywd_data.hlywd_socket, 100); /* Set the playout delay to 100ms */
#endif
    
    /* Set sequence number */
    hlywd_data.seq = seq;

    /* open media file, and decode frames in video_frames structure */
    int ret = 0, got_frame;

    /* register all formats and codecs */
    av_register_all();

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data, video_dst_linesize,
                             width, height, pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto decode_end;
        }
        video_dst_bufsize = ret;
    }

    int vden = fmt_ctx->streams[video_stream_idx]->time_base.den;
    int vnum = fmt_ctx->streams[video_stream_idx]->time_base.num;
    vtb = DIV_ROUND_CLOSEST(vnum * SEC2PICO, vden);

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto decode_end;
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    /* get file size */
    struct stat src_file_stat;
    stat(src_filename, &src_file_stat);
    src_filesize = src_file_stat.st_size;

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

decode_end:
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
        
    /* Initialize the condition and mutex */
    pthread_cond_init(&msg_ready, NULL);
    pthread_mutex_init(&msg_mutex, NULL);
    
    /* Initialize the threads, creating joinable for portability ref: 
     *    https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables
     */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    /* Create parser thread, passing pparams as argument */
    int err = pthread_create(&(av_tid), &attr, &fill_timing_info, (void *) &pparams);
    
    if (err != 0) {
        printf("\ncan't create parser thread :[%s]", strerror(err));
    }
    
    /* Create sender thread, passing hlywd_data as argument */
    err = pthread_create(&(h_tid), &attr, &write_to_hollywood, (void *) &hlywd_data);
    
    if (err != 0) {
        printf("\ncan't create hollywood sender thread :[%s]", strerror(err));
    }
    
    /* Wait for threads to end */
    pthread_join(av_tid, NULL);
    pthread_join(h_tid, NULL);
    
END:
    seq = hlywd_data.seq;
    offset = 0;
    
    /* Destroy the attr, mutex & condition */
    pthread_attr_destroy(&attr);
    pthread_cond_destroy (&msg_ready);
    pthread_mutex_destroy(&msg_mutex);
    
    return seq;
}