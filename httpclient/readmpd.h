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
#include "mm_parser.h"

int read_mpddata(char * memory, char mpdlink[], struct metrics * metric);

#endif /* defined(____readmpd__) */
