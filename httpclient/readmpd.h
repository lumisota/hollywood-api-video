//
//  readmpd.h
//  
//
//  Created by Saba Ahsan on 08/11/16.
//
//

#ifdef __cplusplus
extern "C" {
#endif
    
#ifndef ____readmpd__
#define ____readmpd__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libxml/parser.h>
#include "../common/helper.h"
#include "../common/http_ops.h"


#define MAX_SUPPORTED_BITRATE_LEVELS 24
#define READMPD ""
//#define READMPD "READMPD"


typedef struct
{
    long bitrate;
    char **segments;
} level;


typedef struct
{
    /*DASH params*/
    int num_of_segments;
    int num_of_levels;
    int segment_dur_ms;
    uint8_t init; /*boolean - init segment (with no media data) exists or not*/ 
    level bitrate_level[MAX_SUPPORTED_BITRATE_LEVELS];
    
}manifest;

int read_mpddata(char * memory, char mpdlink[], manifest * m);

#endif /* defined(____readmpd__) */
#ifdef __cplusplus
}
#endif
