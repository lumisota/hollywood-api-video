/*
 * helper.cpp
 *
 *  Created on: Feb 28, 2013
 *      Author: sahsan
 */

int verbose = 0;

#include "helper.h"
#define BIGENDIAN 1
#define LITTLEENDIAN 0

void printdebug(const char* source, const char* format, ... )
{
    if (verbose == 0)
        return;
    
    va_list args;
    if ( strlen(source)==0)
        return; 
    fprintf( stderr,"%s: ", source );
    va_start( args, format );
    vfprintf( stderr, format, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

/*Defined as secondary print function for debug reasons, modify if needed*/
void pprintdebug(const char* source, const char* format, ... )
{
   return; 
}




/*http://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
 * Function returns NULL when there is no replacement to be made.
 */

char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements
//	cout<<"woah boy replacing "<<rep<<" with "<<with;

    if (!orig)
    {
       // printf(" not found 1\n");
        return NULL;
    }
    if (!rep || !(len_rep = strlen(rep)))
    {
      //  printf(" not found 2 \n");
        return NULL;
    }
    if (!(ins = strstr(orig, rep)))
    {
       // printf(" not found 3\n");
        return NULL;
    }
    if (!with)
        with[0] = '\0';
    len_with = strlen(with);
    tmp = strstr(ins, rep);
    for (count = 0; tmp ; ++count) {
        ins = tmp + len_rep;
        tmp = strstr(ins, rep);
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = (char *)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
    {
    	//printf(" not found \n");
        return NULL;
    }
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
//	cout<<" done "<<endl;

    return result;
}


int hostendianness()
{
    char chtest[2]={'a','\0'};
    short itest=0;
    memcpy(&itest, chtest,2);
    if (itest>100)
        return BIGENDIAN;
    else
        return LITTLEENDIAN;
}


uint64_t atouint64 (unsigned char* buf)
{
    int endianness = hostendianness();

    uint64_t res=0;
    if(endianness==LITTLEENDIAN)
    {
        memcpy(&res,(unsigned char*) buf+7,1);
        memcpy((unsigned char*)&res+1,(unsigned char*) buf+6,1);
        memcpy((unsigned char*)&res+2,(unsigned char*) buf+5,1);
        memcpy((unsigned char*)&res+3,(unsigned char*) buf+4,1);
        memcpy((unsigned char*)&res+4,(unsigned char*) buf+3,1);
        memcpy((unsigned char*)&res+5,(unsigned char*) buf+2,1);
        memcpy((unsigned char*)&res+6,(unsigned char*) buf+1,1);
        memcpy((unsigned char*)&res+7,(unsigned char*) buf,1);
    }
    else
    {
        memcpy(&res,(unsigned char*)buf, 8);
    }
    return res;
}

int uint64toa (unsigned char* res, uint64_t val)
{
    int endianness = hostendianness();
    if(endianness==LITTLEENDIAN)
    {
        memcpy(res,(unsigned char*)&val+7,1);
        memcpy((unsigned char*)res+1,(unsigned char*) &val+6,1);
        memcpy((unsigned char*)res+2,(unsigned char*) &val+5,1);
        memcpy((unsigned char*)res+3,(unsigned char*) &val+4,1);
        memcpy((unsigned char*)res+4,(unsigned char*) &val+3,1);
        memcpy((unsigned char*)res+5,(unsigned char*) &val+2,1);
        memcpy((unsigned char*)res+6,(unsigned char*) &val+1,1);
        memcpy((unsigned char*)res+7,(unsigned char*) &val,1);
    }
    else
    {
        memcpy(&res,(unsigned char*)val, 8);
    }
    return 1;
}

uint64_t atouint32 (unsigned char* buf)
{
    int endianness = hostendianness();

    uint64_t res=0;
    if(endianness==LITTLEENDIAN)
    {
        memcpy((unsigned char*)&res,(unsigned char*) buf+3,1);
        memcpy((unsigned char*)&res+1,(unsigned char*) buf+2,1);
        memcpy((unsigned char*)&res+2,(unsigned char*) buf+1,1);
        memcpy((unsigned char*)&res+3,(unsigned char*) buf,1);
    }
    else
    {
        memcpy(&res,(unsigned char*)buf, 4);
    }
    return res;
}

int uint32toa (unsigned char* res, uint64_t val)
{
    int endianness = hostendianness();
    if(endianness==LITTLEENDIAN)
    {
        memcpy((unsigned char*)res,(unsigned char*) &val+3,1);
        memcpy((unsigned char*)res+1,(unsigned char*) &val+2,1);
        memcpy((unsigned char*)res+2,(unsigned char*) &val+1,1);
        memcpy((unsigned char*)res+3,(unsigned char*) &val,1);
    }
    else
    {
        memcpy(&res,(unsigned char*)val, 4);
    }
    return 1;
}





