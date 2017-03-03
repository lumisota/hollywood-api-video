//
//  readmpd.h
//  
//
//  Created by Saba Ahsan on 08/11/16.
//
//

#ifndef ____readmpd__
#define ____readmpd__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libxml/parser.h>
#include "helper.h"
#include "http_ops.h"


#define MAX_SUPPORTED_BITRATE_LEVELS 24

typedef struct
{
    int bitrate;
    char **segments;
} level;


typedef struct
{
    /*DASH params*/
    int num_of_segments;
    int num_of_levels;
    level bitrate_level[MAX_SUPPORTED_BITRATE_LEVELS];
    
}manifest;

int read_mpddata(char * memory, char mpdlink[], manifest * m);

#endif /* defined(____readmpd__) */
