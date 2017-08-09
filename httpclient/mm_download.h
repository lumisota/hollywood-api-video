//
//  mm_download.h
//  
//
//  Created by Saba Ahsan on 22/02/17.
//
//

#ifndef ____mm_download__
#define ____mm_download__

#include <stdio.h>
#include <pthread.h>

#include "mm_parser.h"
#include "../common/http_ops.h"
#include "readmpd.h"
#include "bola.h"
#include "panda.h"

#define DEFAULT_BUFFER_DURATION 16000 /*milliseconds*/

int init_transport(transport * t);

int play_video (struct metrics * metric, manifest * media_manifest , transport * media_transport, long throughput, int algo);

#endif /* defined(____mm_download__) */
