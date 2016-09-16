/*
 * mpeg.h
 *
 *  Created on: Nov 20, 2013
 *      Author: sahsan
 */

#ifndef MPEG_H_
#define MPEG_H_
//#define MP4DEBUG
#define MP4DEBUGA
#define MAXSIZEMP4 8 /*number of bytes used to represent size of an mp4 atom*/
#define MP4IDLEN 4 /*number of bytes used to represent the id of an mp4 atom*/
#define SIZEMVHD 24
#define SIZETKHD 16
#define SIZETFHD 4
#define SIZESTCO 8
#define SIZESTSS 8
#define SIZETRUN 8 /* flags and entries*/
#define STTSENTRYSIZE 8 /*each m->table entry has 8 bytes. 4 for number of samples, and 4 for sample duration*/
#define STTSSAMPLENUMSIZE 4 /*each m->table entry has 8 bytes. 4 for number of samples, and 4 for sample duration*/
#define SIZESTTS 8
#define SIZESIDX 24
#define SIDXENTRYSIZE 12 /*each sidx entry has 16 bytes. 4 for refsize, and 4 for sample duration, 4 for sap delta time*/
#define SIDXSINGLESIZE 4 /*each m->table entry has 8 bytes. 4 for number of samples, and 4 for sample duration*/
#define NUMOFSTREAMS 2 

#define SIZESTSZ 12
#define SIZEMDHD 20
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <iostream>
using namespace std;
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


enum mp4state_t {MREADID, MREADSIZE, MREADBODY, MREADESIZE,MREADSIDX, MREADFTYP, MREADMVHD, MREADMDHD, MREADHDLR, MREADSTSD, MREADTFDT, MREADTRUN, MREADTFHD, MREADSTTS, MREADSTSS, MREADSTSC, MREADSTCO,MREADSTSZ, MREADTKHD,MREADMDAT};

enum mp4level_t {ML0, ML1, ML2, ML3, ML4, ML5 };

enum mp4ftyp {MP42, DASH, MUNKNOWNF};

enum mp4idsl0_t {MOOV, MOOF, MDAT, FTYP, SIDX, MUNKNOWNL0};
enum mp4idsl1_t {MVHD, MFHD, TRAK, TRAF, MUNKNOWNL1};
enum mp4idsl2_t {TKHD, TFHD, MDIA, TRUN, TFDT,  MUNKNOWNL2};
enum mp4idsl3_t {MINF,MDHD,HDLR, MUNKNOWNL3};
enum mp4idsl4_t {STBL, MUNKNOWNL4};
enum mp4idsl5_t {STTS,STSS,STSC,STSZ,STCO,CO64, MUNKNOWNL5};

struct metrics
{
//	videourl url[NUMOFSTREAMS];
//	filetype ft;
	int numofstreams;
//	char link[MAXURLLENGTH];
	time_t htime; /*unix timestamp when test began*/
	long stime; /*unix timestamp in microseconds*/
	long etime;/*unix timestamp in microseconds*/
	//char url[CDNURLLEN];
	int numofstalls;
	double totalstalltime; //in microseconds
	double maxstalltime; //in microseconds
	double initialprebuftime; //in microseconds
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
	char bitratelist_adaptive[512];
	double downloadrate[NUMOFSTREAMS];
	float duration;
	int errorcode;
	int playout_buffer_seconds;
	double connectiontime[NUMOFSTREAMS];
	double firstconnectiontime;

};


struct mp4timeinfo {
	uint32_t prefrate;
	uint32_t timescale;
	uint32_t duration;

};

struct subsegs {
	uint32_t refsize; /*size of subsegment in bytes, 31 bits. 1st bit is ref type, this includes the moof and mdat*/
	uint32_t ssdur;
	uint32_t sapdelta; /*no idea what this is(uses only 28bits)First 4 bits are : 1 Starts with Sap, 3 bits SAP type*/
};

struct tts{
	double stime; /*in milliseconds*/
	uint32_t size;
	uint32_t sdur;
	uint32_t chunk;
	uint64_t offset; /*the last byte of the sample*/
	uint32_t key;
};

struct tinfo{
	uint16_t trackid;
	uint32_t timescale;
	uint32_t duration;
	tts * stable;
	uint32_t keyframes;
	uint32_t entries;
	char handler[5];

};


struct levelmp4info {
	mp4idsl0_t id0;
	mp4idsl1_t id1;
	mp4idsl2_t id2;
	mp4idsl3_t id3;
	uint64_t size0;
	mp4idsl4_t id4;
	uint64_t size4;
	mp4idsl5_t id5;
	uint64_t size5;
	uint64_t size1;
	uint64_t size2;
	uint64_t size3;
	bool extended;
};

struct mp4_i{
	mp4ftyp ftyp;
	unsigned int stream; /*if 0 it's video(STREAM_VIDEO), if 1 it's audio(STREAM_AUDIO)*/
	/*for saving the header of the boxes*/
	unsigned char * header;
	int index;
	int tocopy;
	/*****************/
	mp4state_t mp4state;
	mp4level_t mp4level; /*indicates the level ID that we are looking for*/
	struct levelmp4info li;
	uint32_t timescale;
	uint64_t rxbytesmp4;
	int firstcall;
	mp4timeinfo mp4tinfo;
	map<uint32_t, struct tinfo> tracks;
	int currtrakid;
	map<double, uint64_t> samplemap; /*<starttime of frame, offset (bytes) of frame>*/
	unsigned char * table;
	/* the following values are for adaptive videos*/
	unsigned char * sidx;
	struct subsegs * ss;
	uint32_t defsamplesize;
	uint32_t defsampledur;
	uint64_t decodetime;
	uint32_t segment;
	/************************/
	uint32_t entries; /*used for nonadaptive when reading table, value is used only temporarily when reading the boxes*/
};

struct mp4_i mp4_initialize();
int mp4_get_size (unsigned char * stream, int len, uint64_t * size, bool extended, struct mp4_i * m);
int mp4_get_ID (unsigned char * stream, int len, int * ID, struct mp4_i * m);
bool mp4_setID( struct mp4_i * m);
int mp4_get_body (unsigned char * stream, int len, uint64_t size, struct mp4_i * m);
void checkmp4level( struct mp4_i * m);
int mp4_savetag (unsigned char * data, int len, struct mp4_i * m);
int mp4_read_mvhd(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_mdhd(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_tkhd(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_stts(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_stss(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_stsc(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_stsz(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_stco(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_mdat(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_sidx(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_trun(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_tfhd(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_ftyp(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_mdat_a(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_tfdt(unsigned char * stream, int len, struct mp4_i * m);
int mp4_read_hdlr(unsigned char * stream, int len, struct mp4_i * m);
void mp4_destroy(struct mp4_i * m);


/*from helper.cpp*/
char *str_replace(char *orig, char *rep, char *with);
bool hostendianness();
uint32_t atouint24 (char * buf, bool endianness);
uint16_t atouint16 (char * buf, bool endianness);
uint32_t reverse_chartouint32(char * buf, bool endianness);
bool getdatetimestr(char * htime);
long gettimelong();
void memzero (void * ptr, int size);
uint32_t atouint32 (char * buf, bool endianness);
uint64_t atouint64 (char * buf, bool endianness);

#endif /* MPEG_H_ */
