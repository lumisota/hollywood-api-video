/*
 * youtube-dl.h
 *
 *  Created on: Feb 28, 2013
 *      Author: sahsan
 */

#ifndef YOUTUBE_DL_H_
#define YOUTUBE_DL_H_
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iomanip>

#include <iostream>
using namespace std;
#include <pthread.h>
#include <curl/curl.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#define LOGFILE "logfile.txt"

#include "flvreader.h"


/*the error codes*/
//#define FULLHD 0
//#define DEBUG 1
#define ITWORKED 600
#define WERSCREWED 666
#define BADFORMATS 601 //we didn't find the right format for download.
#define MAXDOWNLOADLENGTH 603 /*three minutes of video has been downloaded*/
#define MAXTESTRUNTIME 604 /*test has been downloading video for the maximum time */
#define SIGNALHIT 605 /* program received ctrl+c*/
#define CODETROUBLE 606 /*HTTP code couldn't be retrieved*/
#define ERROR_STALL 607 /* There was a stall */
#define TOO_FAST 608 /* The test finsihed too fast */
#define PARSERROR 609 /*There was an error with the initial HTTP response*/
#define CURLERROR 610
#define CURLERROR_GETINFO 611
#define FIRSTRESPONSERROR 620

#define NUMOFSTREAMS 2
#define STREAM_VIDEO 0
#define STREAM_AUDIO 1



#define NUMOFCODES 9
#define MP4FORMAT720 22 /*720p mp4*/
#define MP4FORMAT360 18 /*360p mp4*/
#define FLVFORMAT 34 /*360p flv*/
//#define WEBMFORMAT 43 /*360x640 webm*/
#define WEBMFORMAT 46 /*1080x1920 webm*/
#define WEBMFORMAT720 45 /*720x webm*/
#define MIN_THROUGHPUT_REPORT_TIME 1 /* in second*/
#define MIN_PREBUFFER 2000 /* in millisecond*/
#define MIN_STALLBUFFER 1000
#define DECODE_TIME 50000 /* the time required for the packet to be decoded and rendered microseconds*/
#define MAXURLLENGTH 512
#define LEN_PLAYOUT_BUFFER 40 /*Length of the playout buffer in seconds*/
#define LEN_CHUNK_FETCH 1 /*Length to be requested to refill buffer*/
#define LEN_CHUNK_MINIMUM 5 /*Shortest length of the buffer*/

/*
 * If condition given by the parameter is NOT met, the macro
 * will print info about the error condition and exit the process.
 *
 */

#define CHECK(EXPR) do {\
   if (!((EXPR) + 0)) {\
     perror(#EXPR);\
     fprintf(stderr,"CHECK: Error on process %d file %s line %d\n",\
	     getpid(), __FILE__,__LINE__);\
     fputs("CHECK: Failing assertion: " #EXPR "\n", stderr);\
     exit(1);\
   }\
} while (0)

/*the number of filetype enum indicates priority, the lower the value, the higher the priority*/
enum filetype {MP4=2, WEBM=3, FLV=4, MP4_A=1, NOTSUPPORTED=5};

#define URLTYPELEN 100
#define URLSIZELEN 12
#define URLSIGLEN 100
#define CDNURLLEN 1500
#define URLLISTSIZE 24

typedef struct
{
	int itag;
	char url[CDNURLLEN];
	char sig[URLSIGLEN];
	char type[URLTYPELEN];
	char size[URLSIZELEN];
	int bitrate;
	long range0;
	long range1;
	bool playing;
} videourl;

struct stream_metrics
{
	uint64_t TSnow; //TS now (in milliseconds)
	uint64_t TS0; //TS when prebuffering started (in milliseconds). Would be 0 at start of movie, but would be start of stall time otherwise when stall occurs.
	long Tmin; // microseconds when prebuffering done or in other words playout began.
	long Tmin0; //microseconds when prebuffering started, Tmin0-Tmin should give you the time it took to prebuffer.
	long T0; /*Unix timestamp when first packet arrived in microseconds*/
	double downloadtime; //time take for video to download
};

#define CHARPARSHORT 12
#define CHARPARLENGTH 24
#define CHARSTRLENGTH 256
#define FORMATLISTLEN 100


struct metrics
{
	videourl url[NUMOFSTREAMS];
	filetype ft;
	int numofstreams;
	char link[MAXURLLENGTH];
	time_t htime; /*unix timestamp when test began*/
	long stime; /*unix timestamp in microseconds*/
	long etime;/*unix timestamp in microseconds*/
	//char url[CDNURLLEN];
	int numofstalls;
	char cdnip[NUMOFSTREAMS][CHARSTRLENGTH];
	double totalstalltime; //in microseconds
	double maxstalltime; //in microseconds
	double initialprebuftime; //in microseconds
	double downloadtime[NUMOFSTREAMS]; //time take for video to download
	double totalbytes[NUMOFSTREAMS];
	double maxmediarate; //highest inst. media rate observed (kbps)
	float videorate;
	float audiorate;
	double totalrate;
	uint64_t TSnow; //TS now (in milliseconds)
	uint64_t TSlist[NUMOFSTREAMS]; //TS now (in milliseconds)
	uint64_t TS0; //TS when prebuffering started (in milliseconds). Would be 0 at start of movie, but would be start of stall time otherwise when stall occurs.
	long Tmin; // microseconds when prebuffering done or in other words playout began.
	long Tmin0; //microseconds when prebuffering started, Tmin0-Tmin should give you the time it took to prebuffer.
	long T0; /*Unix timestamp when first packet arrived in microseconds*/
	int dur_spec; /*duration in video specs*/
	char category[CHARPARLENGTH];
	char views[CHARPARLENGTH];
	char title[CHARSTRLENGTH];
	char formatlist[FORMATLISTLEN];
	char formatlist_adaptive[FORMATLISTLEN];
	char bitratelist_adaptive[512];
	char likes[CHARPARLENGTH];
	char dislikes[CHARPARLENGTH];
	char date[CHARPARLENGTH];
	char dimension[CHARPARSHORT];
	double downloadrate[NUMOFSTREAMS];
	float duration;
	int errorcode;
	int playout_buffer_seconds;
	double connectiontime[NUMOFSTREAMS];
	double firstconnectiontime;

};



int downloadfiles(videourl url []);
long getfilesize(videourl vdourl, int format);

int extract_bitrate(char *data , int *bitrate);
int extract_manifestlink(char * data , char * manifest);
int extract_urlstring_adaptive(char * data, int * start, int * finish);
int split_urls(char * data, videourl url_list[], char * formatlist, bool adaptive);


int find_urls_adaptive(char * data, videourl * url, bool getsizes, bool prefflv);
int findbestformat(int bestsize, videourl * url, bool adaptive, videourl * url_list, int index);
int findprefformat( videourl * url, videourl * url_list, char *  Preflist, char * formatlist, int index);

char *str_replace(char *orig, char *rep, char *with);
bool hostendianness();
uint32_t atouint24 (char * buf, bool endianness);
uint16_t atouint16 (char * buf, bool endianness);
uint32_t reverse_chartouint32(char * buf, bool endianness);
bool getdatetimestr(char * htime);
long gettimelong();
void printstats(char * url, int status, char * vdourl);
time_t gettimeshort();
void memzero (void * ptr, int size);
uint32_t atouint32 (char * buf, bool endianness);
void printheaders(char * url, char * vdourl);
void printvalues();
uint64_t atouint64 (char * buf, bool endianness);
int16_t atoint16 (char * buf, bool endianness);
void checkstall(bool);
size_t save_file(void *ptr, size_t size, size_t nmemb, void *stream);
void printspecs(char * url, int status);
int find_urls(char * data, videourl * url, bool getsizes, char *  Preflist, bool adaptive, char * PrefAVlist);
bool replace_html_codes(char * data);
void mainexit();
int progress_func(void* ptr, double TotalToDownload, double NowDownloaded,
                    double TotalToUpload, double NowUploaded);

int startlog();
int writelog(char * logbuffer, int datalen, int logfd);
int closelog(int logfd);
int readconfig(char * youtubelink);
void printhelp(char * name);
void sigint_handler(int sig);
struct timeval get_curr_playoutbuf_len_forstream(int i);
struct timeval get_curr_playoutbuf_len();

void printvalues2();

#ifdef __linux__
int socketer(void *clientp, curl_socket_t item);
#endif /* linux */
#endif /* YOUTUBE_DL_H_ */
