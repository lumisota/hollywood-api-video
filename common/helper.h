/*
 * helper.h
 *
 *  Created on: Oct 24, 2013
 *      Author: sahsan
 */

#ifndef HELPER_H_
#define HELPER_H_

#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <stdarg.h>

char *str_replace(char *orig, char *rep, char *with);
void printdebug(const char* source, const char* format, ... );


static inline void memzero (void * ptr, int size)
{
	memset(ptr, 0, size);
}

static inline long long gettimelong()
{
	struct timeval start;

	gettimeofday(&start, NULL);
	return((long long)start.tv_sec * 1000000 + start.tv_usec);
}

static inline time_t gettimeshort()
{
	struct timeval start;

	gettimeofday(&start, NULL);
	return start.tv_sec;
}

#endif /* HELPER_H_ */
