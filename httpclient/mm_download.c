//
//  mm_download.c
//  
//
//  Created by Saba Ahsan on 22/02/17.
//
//

#include "mm_download.h"


int play_video (struct metrics * metric )
{
    int curr_segment                = 0;
    int curr_bitrate_level          = 0;
    char * curr_url[MAXURLLENGTH]   = NULL;

    metric.stime = gettimelong();
    printf("Starting download 1\n"); fflush(stdout);
    
    while (curr_segment != metric.num_of_segments)
    {
        curr_url = metric->bitrate_level[curr_bitrate_level].segments[curr_segment];
        if(send_get_request ( metric->sock, curr_url, metric->Hollywood)<0)
            return -1;
    }



}