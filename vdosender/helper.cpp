/*
 * helper.cpp
 *
 *  Created on: Feb 28, 2013
 *      Author: sahsan
 */


#include <cstdlib>
#include <cstring>
#include <cstdio>
using namespace std;
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

#define BIGENDIAN 1
#define LITTLEENDIAN 0

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
//    	cout<<" not found "<<endl;
        return NULL;
    }
    if (!rep || !(len_rep = strlen(rep)))
    {
//    	cout<<" not found "<<endl;
        return NULL;
    }
    if (!(ins = strstr(orig, rep)))
    {
//    	cout<<" not found "<<endl;
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
//    	cout<<" not found "<<endl;
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

void memzero (void * ptr, int size)
{
	memset(ptr, 0, size);
}

long gettimelong()
{
	struct timeval start;

	gettimeofday(&start, NULL);
	return(start.tv_sec * 1000000 + start.tv_usec);
}

time_t gettimeshort()
{
	struct timeval start;

	gettimeofday(&start, NULL);
	return start.tv_sec;
}

bool getdatetimestr(char * htime)
{
  time_t ltime;
  struct tm *ltmtime;

  if((ltime=time((time_t *)NULL)) < 0 )
	  return false;

  ltmtime = localtime(&ltime);
  if(ltmtime==NULL)
		return false;

  if(strftime(htime,50,"%A %d %B %Y %H:%M:%S",ltmtime) == 0)
	  return false;
  else
	  return true;

}


bool hostendianness()
{
    char chtest[2]={'a','\0'};
    short itest=0;
    memcpy(&itest, chtest,2);
	if (itest>100)
		return BIGENDIAN;
	else
		return LITTLEENDIAN;
}

/*converts 4 byte char array to uint32*/
uint32_t atouint32 (char * buf, bool endianness)
{
	uint32_t res=0;
	if(endianness==LITTLEENDIAN)
 	{
		 memcpy(&res,(unsigned char*) buf+3,1);
		 memcpy((char*)&res+1,(unsigned char*) buf+2,1);
		 memcpy((char*)&res+2,(unsigned char*) buf+1,1);
		 memcpy((char*)&res+3,(unsigned char*) buf,1);
	}
	else
	{
		memcpy(&res,(unsigned char*)buf, 4);
 	}
	return res;

}


/*converts 8 byte char array to uint64*/
uint64_t atouint64 (char * buf, bool endianness)
{
	uint64_t res=0;
	if(endianness==LITTLEENDIAN)
 	{
		 memcpy(&res,(unsigned char*) buf+7,1);
		 memcpy((char*)&res+1,(unsigned char*) buf+6,1);
		 memcpy((char*)&res+2,(unsigned char*) buf+5,1);
		 memcpy((char*)&res+3,(unsigned char*) buf+4,1);
		 memcpy((char*)&res+4,(unsigned char*) buf+3,1);
		 memcpy((char*)&res+5,(unsigned char*) buf+2,1);
		 memcpy((char*)&res+6,(unsigned char*) buf+1,1);
		 memcpy((char*)&res+7,(unsigned char*) buf,1);
	}
	else
	{
		memcpy(&res,(unsigned char*)buf, 8);
 	}
	return res;
}

/*converts 3 byte char array to uint32*/
uint32_t atouint24 (char * buf, bool endianness)
{
	uint32_t res=0;
	if(endianness==LITTLEENDIAN)
 	{
		 memcpy((char*)&res,(unsigned char*) buf+2,1);
		 memcpy((char*)&res+1,(unsigned char*) buf+1,1);
		 memcpy((char*)&res+2,(unsigned char*) buf,1);
	}
	else
	{
		memcpy((char*)&res+1,(unsigned char*)buf, 3);
 	}
	return res;

}

/*converts 2 byte char array to int16*/
int16_t atoint16 (char * buf, bool endianness)
{
	int16_t res=0;
	if(endianness==LITTLEENDIAN)
 	{
		 memcpy((char*)&res,(unsigned char*) buf+1,1);
		 memcpy((char*)&res+1,(unsigned char*) buf,1);
	}
	else
	{
		memcpy(&res,(unsigned char*)buf, 2);
 	}
	return res;

}

/*converts 2 byte char array to uint16*/
uint16_t atouint16 (char * buf, bool endianness)
{
	uint16_t res=0;
	if(endianness==LITTLEENDIAN)
 	{
		 memcpy((char*)&res,(unsigned char*) buf+1,1);
		 memcpy((char*)&res+1,(unsigned char*) buf,1);
	}
	else
	{
		memcpy(&res,(unsigned char*)buf, 2);
 	}
	return res;

}

/*copies a char to uint32 at the most significant byte*/
uint32_t reverse_chartouint32(char * buf, bool endianness)
{
	uint32_t res=0;
	if(endianness==LITTLEENDIAN)
 	{
		 memcpy((char*)&res+3,(unsigned char*) buf,1);
	}
	else
	{
		memcpy(&res,(unsigned char*)buf, 1);
 	}
	return res;

}


