#ifndef MM_PARSER_H_
#define MM_PARSER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "helper.h"
#include "../lib/hollywood.h"


struct metrics {
    int Hollywood;
    int sock;
    FILE * fptr;
    hlywd_sock h_sock;
};

int mm_parser(struct metrics * m);

#endif
