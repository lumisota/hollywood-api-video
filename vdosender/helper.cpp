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


void printhelp(char * name)
{
	printf("You have used the --help switch so here is the help:\n\n");

	printf("The program uses the configuration file configyt and default values to run, alternatively arguments can be provided to override both configyt and defaults:\n\n");

	printf("Usage of the program:\n");
	printf("\n");
	printf("%s <optional-parameters>\n", name);
	printf("\n");
	printf("Optional parameters\n");
	printf("\t--nodownload => Only get the video information, actual video is not downloaded\n");
	printf("\n");
	printf("\t--verbose => Print instantaneous metric values when downloading video\n\t\tHas no effect if --nodownload is used\n");
	printf("\n");
	printf("\t--getsizes => Query the file size of each of the available formats and print them\n");
	printf("\n");
	printf("\t--flv => Download the default FLV version even if other formats are available\n");

	printf("\nThe output contains some information about the test and also various kinds of records that can be identified by the first column of the record"
			"which can be 1, 2, 3 etc.. The fields are given below\n\n");
	printf("Type: 1 or 0 ; printed before downloading the video. If the video URLs were extracted successfully it is 1 else 0\n");
	printf("1\turl\tcategory\tduration\tviews\tlikes\tdislikes\tformatlist\tadaptiveavailable\ttitle\tuploaddate\n\n");

	printf("Type: 2 prints the final metrics for the downloaded video\n");
	printf("2\tDownloadTime\tNumberOfStalls\tAverageStallDuration\tTotalStallTime\tAveThroughput\tVideoRate\tAudioRate\tDuration\tInitialPrebufferingtime(us)\t"
			"TotalMediaRate\tTotalBytes\tMaxMediaRate\n\n");

	printf("Type: 3 prints some instantaneous metrics every second ONLY if --verbose is used \n");
	printf("3;Timenow;LastTSreceived;PercentageOfVideoDownloaded;TotalbytesDownloaded;TotalBytestoDownload;InstantenousThrouput;"
			"AverageThroughput;AverageMediaRate;NumberofStalls;AverageStallDuration;TotalStallTime\n\n");

	printf("Type: 6 prints the instantaneous mediarate \n");
	printf("6\tMediarate(kbps)\tTimestampNow(microseconds)\tCumulativeMediarate(kbps)\n\n");

	printf("Type: 12 prints the stall information for each stall (Times in milliseconds) \n");
	printf("12\tStall-starttime(videoplayout)\tStallDuration(msec)\n\n");

	printf("Type: 4 prints the TCP stats of video download \n");
	printf("4\ttcp_info.tcpi_last_data_sent\t\
		tcp_info.tcpi_last_data_recv\t\
		tcp_info.tcpi_snd_cwnd\t\
		tcp_info.tcpi_snd_ssthresh\t\
		tcp_info.tcpi_rcv_ssthresh\t\
		tcp_info.tcpi_rtt\t\
		tcp_info.tcpi_rttvar\t\
		tcp_info.tcpi_unacked\t\
		tcp_info.tcpi_sacked\t\
		tcp_info.tcpi_lost\t\
		tcp_info.tcpi_retrans\t\
		tcp_info.tcpi_fackets\n"
	   );


	printf("When the --getsizes option is used the sizes are printed out one line for each format as follows  \n");
	printf("filesize\titag\tfilesize(bytes)\tcdnip\n\n");

	printf("DO NOT ATTEMPT TO CHANGE configyt IF YOU DON't KNOW WHAT YOU ARE DOING.\n");
	printf("RUN WITHOUT ARGUMENTS FOR TESTING.\n");

}

