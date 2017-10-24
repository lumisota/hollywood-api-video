#ifdef __cplusplus
extern "C" {
#endif
    

#ifndef MM_PARSER_H_
#define MM_PARSER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <pthread.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include "../common/helper.h"
#include "../lib/hollywood.h"
#include "playout_buffer.h"
#include "readmpd.h"
#include "../common/http_ops.h"

#define MIN_PREBUFFER 2000 /* in millisecond*/
#define NUMOFSTREAMS 1
#define MAX_DASH_INIT_SEGMENT_SIZE 1000
#define URLLISTSIZE 24

enum parser_status {
    P_STARTUP = 0, 
    P_STANDBY = 1, 
    P_READY = 2, 
    P_RUNNING = 3 
};

enum stream_type {
    STREAM_VIDEO = 0,
    STREAM_AUDIO = 1
};

typedef struct {
    enum parser_status p_status;
    int last_parsed_segment;                 
    pthread_cond_t  queue_ready;      /*indicates that init segment has been downloaded*/
    pthread_cond_t  msg_ready;      /*indicates that a new message has been received*/
    pthread_mutex_t msg_mutex;      /*mutex of the message*/
    uint8_t Hollywood;
    uint8_t OO;
    int sock;
    FILE * fptr;
    hlywd_sock h_sock;
    uint8_t stream_complete;
    uint8_t parser_exited;
    char port[6];
    char path[380];
    char host[128];
    uint64_t playout_time;
    uint8_t loss_alert; 
    struct playout_buffer * rx_buf;
    uint64_t rtt;
    uint64_t bytes_rx;
    uint64_t download_time;
    int algo; 
}transport;


struct metrics
{
    transport * t;
    long minbufferlen; /*buffer len when playout begins*/
    /*vidoe metrics*/
    long long htime; /*unix timestamp when test began*/
    long long stime; /*unix timestamp in microseconds, when media downlaod began*/
    long long etime;/*unix timestamp in microseconds, when test ended*/
    double startup; /*time in microseconds, from start of test to playout*/
    int numofstalls;
    double totalstalltime; //in microseconds
    double initialprebuftime; //in microseconds
    double totalbytes[NUMOFSTREAMS];
    uint64_t TSnow; //TS now (in milliseconds)
    uint64_t TSlist[NUMOFSTREAMS]; //TS now (in milliseconds)
    uint64_t TS0; //TS when prebuffering started (in milliseconds). Would be 0 at start of movie, but would be start of stall time otherwise when stall occurs.
    long long Tplay; // microseconds when prebuffering done or in other words playout began.
    long long Tempty; //microseconds when prebuffering started, Tempty-Tplay should give you the time it took to prebuffer.
    long long T0; /*Unix timestamp when first packet arrived in microseconds*/
    int playout_buffer_seconds;

};

//typedef struct
//{
//    uint8_t data[MAX_DASH_INIT_SEGMENT_SIZE];
//    uint32_t size;
//} dash_init_segment;



int stall_imminent(struct metrics * metric);
void printmetric(struct metrics metric, transport media_transport );
void * mm_parser(void * opaque);
void checkstall(int end, struct metrics * metric);
int init_metrics(struct metrics *metric);
#endif
#ifdef __cplusplus
}
#endif
