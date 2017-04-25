/*
 * Based on tsdemux (Copyright (C) 2009 Anton Burdinuk; MIT licence)
 */
 
#ifndef TSDEMUX_H
#define TSDEMUX_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "media_sender.h"

#define DIV_ROUND_CLOSEST(x, divisor)(                  \
{                                                       \
        typeof(divisor) __divisor = divisor;            \
        (((x) + ((__divisor) / 2)) / (__divisor));      \
}                                                       \
)

typedef struct h264counter {
    uint32_t ctx;
    uint64_t frame_num;
} h264counter;

typedef struct ac3counter {
    uint16_t st;
    uint32_t ctx;
    uint16_t skip;
    uint64_t frame_num;
} ac3counter;

typedef struct table {
    char buf[512];
    uint16_t len;
    uint16_t offset;
} table;

typedef struct stream {
    uint16_t channel;
    uint8_t id;
    uint16_t type;
    uint8_t stream_id;
    
    table psi;
    
    uint64_t dts;
    uint64_t first_dts;
    uint64_t first_pts;
    uint64_t last_pts;
    uint32_t frame_length;
    uint64_t frame_num;
    
    h264counter frame_num_h264;
    ac3counter frame_num_ac3;
    
    /* linked list metadata */
    uint16_t pid;
    struct stream *next;
} stream;
    
typedef struct parse_attr {
    FILE * fptr;
    struct hlywd_attr *hlywd_data;
    char *src_filename;
    stream *streams;
} parse_attr;

typedef struct vid_frame {
    int starts_at;
    size_t len;
    long unsigned timestamp;
    int key_frame;
    struct vid_frame *next;
} vid_frame;

int validate_type(uint8_t type);
uint8_t to_byte(const char* p);
uint16_t to_int(const char* p);
uint64_t decode_pts(const char* ptr);
stream *get_stream(uint16_t pid);
vid_frame *get_frames(struct parse_attr *p);

#endif