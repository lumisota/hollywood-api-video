#ifndef MM_PARSER_H_
#define MM_PARSER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "helper.h"
#include "../lib/hollywood.h"
#include "buffer.h"

#define MIN_PREBUFFER 2000 /* in millisecond*/
#define MIN_STALLBUFFER 1000
#define NUMOFSTREAMS 1
#define MAX_DASH_INIT_SEGMENT_SIZE 1000
#define MAX_SUPPORTED_BITRATE_LEVELS 24
#define URLLISTSIZE 24


enum stream_type {
    STREAM_VIDEO = 0,
    STREAM_AUDIO = 1
};

typedef struct
{
    uint8_t data[MAX_DASH_INIT_SEGMENT_SIZE];
    uint32_t size;
} dash_init_segment;


typedef struct
{
    int bitrate;
    char **segments;
    dash_init_segment init;
} manifest;


struct metrics
{
    /*Hollywood params*/
    uint8_t Hollywood;
    int sock;
    FILE * fptr;
    hlywd_sock h_sock;
    struct mm_buffer mm_buf;
    uint8_t packets_queued;
    uint32_t lowest_seq_num;
    
    /*DASH params*/
    int num_of_segments;
    int num_of_levels;
    manifest bitrate_level[MAX_SUPPORTED_BITRATE_LEVELS];

    
    /*vidoe metrics*/
    long long htime; /*unix timestamp when test began*/
    long long stime; /*unix timestamp in microseconds, when sending GET request*/
    long long etime;/*unix timestamp in microseconds, when test ended*/
    double startup; /*time in microseconds, from start of test to playout*/
    int numofstalls;
    double totalstalltime; //in microseconds
    double initialprebuftime; //in microseconds
    double totalbytes[NUMOFSTREAMS];
    uint64_t TSnow; //TS now (in milliseconds)
    uint64_t TSlist[NUMOFSTREAMS]; //TS now (in milliseconds)
    uint64_t TS0; //TS when prebuffering started (in milliseconds). Would be 0 at start of movie, but would be start of stall time otherwise when stall occurs.
    long long Tmin; // microseconds when prebuffering done or in other words playout began.
    long long Tmin0; //microseconds when prebuffering started, Tmin0-Tmin should give you the time it took to prebuffer.
    long long T0; /*Unix timestamp when first packet arrived in microseconds*/
    int playout_buffer_seconds;
};


int stall_imminent(struct metrics * metric);
void printmetric(struct metrics metric);
int mm_parser(struct metrics * m);
void checkstall(int end, struct metrics * metric);
int init_metrics(struct metrics *metric);
#endif
