/*
 * mpeg.cpp
 *
 *  Created on: Nov 7, 2013
 *      Author: sahsan
 *      Based on Quick Time File Format Specification
 *      https://developer.apple.com/library/mac/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html#//apple_ref/doc/uid/TP40000939-CH204-25706
 *	 Update: Sep 28, 2016
 *      Code contains hacks added for the TCP Hollywood testing case. Search for term HACK to locate and fix when/if needed. 
 *      All changes/updates/comments for the TCP Hollywood implementation are added with comment term "HOLLY"
 */
#include "mpeg.h"


/* TODO: add a check for compressed data
 * TODO: add a check that all the components of stable have been received
  */

extern bool endianness;
extern metrics metric;
extern bool Mp4Model;


/* The calling function ensures that the pointer data, points to the start of the field to be read.
 * The return value is the number of bytes that have been read/ or should be skipped.
 * This function does not manage state (except for readesize), it is changed inside the called mp4_get_x functions.
 * This function should keep level though.
 */
#include "mpeg.h"
void mp4_destroy(struct mp4_i * m)
{
#ifdef MP4DEBUG
	cout<<"Exiting MP4\n";
#endif

	for( map<uint32_t, struct tinfo>::iterator ii=m->tracks.begin(); ii!=m->tracks.end(); ++ii)
		delete [] (*ii).second.stable;
	m->tracks.clear();
	delete [] m->table;
	if(m->ss!=NULL)
		delete [] m->ss;
	return;
}

struct mp4_i mp4_initialize()
{
	struct mp4_i m;
	m.stream=1; /*HACK: since we are only using one stream(video) for now, 
			so hardcoding this value. Should be set dynamically otherwise*/
	m.mp4state=MREADSIZE;
	m.mp4level=ML0; /*indicates the level ID that we are looking for*/
	m.timescale = 0;
	m.rxbytesmp4=0;
	m.firstcall=1;
	m.currtrakid=0;
	m.table=NULL;
	m.decodetime=0;
	/*this is for the m->header of the boxes
	 * new memory is allocated depending on size of m->header when a box is received.
	 */
	m.header=NULL;
	m.tocopy = -1;/* number of bytes of the MP4 size that remain to be read from the data,
                             -1 if not known yet. */

	m.index=0; /* The write position inside m->header */
	m.ss=NULL;
	return m;
}

/* does not update the mp4_i structure, it is a standalone operation
 * returns 1 if it is an mdat block, else 0, sets m->msg_size*/ 
int get_msg_size (unsigned char * data, int len, struct hlywd_message * m)
{
	int bytesread=0;
	uint64_t size=0;
	int headersize=8; int ID=0;

	m->msg_size = atouint32 ((char *)data, endianness);	
	if (m->msg_size ==1) /*extended size, extract the rest of it*/ 
	{
		m->msg_size = atouint64 ((char *)data+8, endianness); /*8 bytes of size+id*/
		headersize=16;
	}

	m->lifetime_ms = 10000; /*setting max lifetime 10s for messages that are not to be lost*/
	return headersize; 
}


int mp4_savetag (unsigned char * data, int len, struct mp4_i * m)
{
	/* bytesread is the number of bytes that have been read as part of the mp4_get_ID or datasize
	 * incase of readbody it will also contain the bodylength that is to be skipped.
	 */

	int bytesread=0;
	uint64_t size=0;
	int ret=0; int ID=0;
	m->rxbytesmp4+=len;
	if(m->firstcall)
	{
//		atexit(mp4_destroy);
		m->firstcall=0;
	}
	//cout<<"saving tag "<<m->mp4state<<endl;
	while(1)
	{
		switch(m->mp4state)
		{
			case MREADSIZE:
                //printf("state is READSIZE\n");
                //printf("Bytesread calling datasize %d \n", bytesread);
				ret= mp4_get_size(data+bytesread, len-bytesread, &size, false,m);
				break;
			case MREADESIZE:
                //printf("state is READSIZE\n");
                //printf("Bytesread calling datasize %d \n", bytesread);
				ret= mp4_get_size(data+bytesread, len-bytesread, &size, true,m);
				break;

			case MREADID:
				//printf("state is READID\n");
                //	printf("Bytesread calling getid %d \n", bytesread);
				ret= mp4_get_ID(data+bytesread, len-bytesread, &ID,m);
                //	printf("return value %d, %d \n", ret, bytesread);

				break;
			case MREADBODY:
                //				//printf("state is READBODY\n");
				if(size==1)
					m->mp4state=MREADESIZE;
				else
				{
					ret= mp4_get_body(data+bytesread, len-bytesread, size,m);
				}
		//		cout<<"level: "<<m->mp4level<<" size = "<<m->li.size0<<" "<<m->li.size1<<" "<<m->li.size2<<" "<<m->li.size3<<" "<<m->li.size4<<" "<<m->li.size5<<endl;
				break;
			case MREADMVHD:
				ret=mp4_read_mvhd(data+bytesread, len-bytesread,m);
				break;
			case MREADSIDX:
				ret=mp4_read_sidx(data+bytesread, len-bytesread,m);
				break;
			case MREADTKHD:
				ret=mp4_read_tkhd(data+bytesread, len-bytesread,m);
				break;
			case MREADMDHD:
				ret=mp4_read_mdhd(data+bytesread, len-bytesread,m);
				break;
			case MREADSTTS:
				ret=mp4_read_stts(data+bytesread, len-bytesread,m);
				break;
			case MREADSTSS:
				ret=mp4_read_stss(data+bytesread, len-bytesread,m);
				break;
			case MREADSTSZ:
				ret=mp4_read_stsz(data+bytesread, len-bytesread,m);
				break;
			case MREADSTCO:
				ret=mp4_read_stco(data+bytesread, len-bytesread,m);
				break;
			case MREADSTSC:
				ret=mp4_read_stsc(data+bytesread, len-bytesread,m);
				break;
			case MREADMDAT:
				if(m->ftyp == DASH || m->ftyp == ISO5 )
					ret=mp4_read_mdat_a(data+bytesread, len-bytesread,m);
				else
					ret=mp4_read_mdat(data+bytesread, len-bytesread,m);
				break;
            case MREADTRUN:
                ret=mp4_read_trun(data+bytesread, len-bytesread,m);
                break;
            case MREADTREX:
                ret=mp4_read_trex(data+bytesread, len-bytesread,m);
                break;
            case MREADMEHD:
                ret=mp4_read_mehd(data+bytesread, len-bytesread,m);
                break;
			case MREADTFHD:
				ret=mp4_read_tfhd(data+bytesread, len-bytesread,m);
				break;
			case MREADTFDT:
				ret=mp4_read_tfdt(data+bytesread, len-bytesread,m);
				break;
			case MREADFTYP:
				ret=mp4_read_ftyp(data+bytesread, len-bytesread,m);
				break;
			case MREADHDLR:
				ret=mp4_read_hdlr(data+bytesread, len-bytesread,m);
				break;
			default:
				ret=-1;printf("Error: m->mp4state value is not recognized in mpeg4 reader\n");
				break;
		}
		if(ret<0)
		{
			mp4_destroy(m);
			return -1;
		}
//		fwrite(data, sizeof(data[0]), sizeof(x)/sizeof(x[0]), fp);
		bytesread+=ret;
        //	printf("Bytesread %d, %d \n", len, bytesread);
		if(bytesread>=len)
			break;
	}
	return bytesread;
}


/* return : the number of bytes that were read by the function.
 *
 */
int mp4_get_size (unsigned char * stream, int len, uint64_t * size, bool extended, struct mp4_i * m)
{

	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		if(extended)
			m->tocopy=8;
		else
			m->tocopy = 4;
		m->header = new unsigned char [MAXSIZEMP4];
		memzero((void *)m->header, MAXSIZEMP4);
		if(m->tocopy<0 || m->header==NULL)
			return -1;
		m->index=MAXSIZEMP4-m->tocopy;
        //		printf("values are now set, m->tocopy = %d, m->index=%d, flag=%8x", m->tocopy, m->index, flag);
		if(m->mp4level==ML1 )
			m->li.size0-=m->tocopy;
		else if (m->mp4level==ML2)
			m->li.size1-=m->tocopy;
		else if (m->mp4level==ML3)
			m->li.size2-=m->tocopy;
		else if (m->mp4level==ML4)
			m->li.size3-=m->tocopy;
		else if(m->mp4level==ML5)
			m->li.size4-=m->tocopy;
	}
	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	*size = atouint64 ((char *)m->header, endianness);
	ret = m->tocopy;


//    printf("Body size: %llu, %d, [%x%x%x%x]\t", *size, ret, m->header[0],m->header[1], m->header[2], m->header[3]);
 //   	printf("Body size: %llu, %d, [%x%x%x%x]\t", *size, ret, stream[0],stream[1], stream[2], stream[3]);

	if(extended)
	{
		m->mp4state=MREADBODY;
		*size=*size-16; /*take out the 8 bytes of the extended size and the 8 bytes of size+ID*/
	}
	else
	{
		if(*size>1)
			*size=*size-8; /*take out the 8 bytes of size+ID*/
		m->mp4state=MREADID;
	}
	if(m->mp4level==ML0)
		m->li.size0=*size;
	else if(m->mp4level==ML1)
		m->li.size1=*size;
	else if(m->mp4level==ML2)
		m->li.size2=*size;
	else if(m->mp4level==ML3)
		m->li.size3=*size;
	else if(m->mp4level==ML4)
			m->li.size4=*size;
	else if(m->mp4level==ML5)
			m->li.size5=*size;
	else
		return -1;

	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	return ret;
}



/* return : the number of bytes that were read by the function.
 *
 */
int mp4_get_ID (unsigned char * stream, int len, int * ID, struct mp4_i * m)
{
	int ret =len;

	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy = MP4IDLEN;
		m->header = new unsigned char [m->tocopy];

		if(m->tocopy<0 || m->header==NULL)
			return -1;
		m->index=0;
		if(m->mp4level==ML1 )
			m->li.size0-=m->tocopy;
		else if (m->mp4level==ML2)
			m->li.size1-=m->tocopy;
		else if (m->mp4level==ML3)
			m->li.size2-=m->tocopy;
		else if (m->mp4level==ML4)
			m->li.size3-=m->tocopy;
		else if(m->mp4level==ML5)
			m->li.size4-=m->tocopy;
	}

	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	ret = m->tocopy;

	mp4_setID(m);
	m->mp4state=MREADBODY;
	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	return ret;
}


bool mp4_setID( struct mp4_i * m)
{
	/* Segment ID [18][53][80][67] - ML0
	 * 	 	Segment Info ID [15][49][A9][66]- ML1
	 * 			Timecode Scale ID [2A][D7][B1]- ML2
	 * 	 	Cluster ID [1F][43][B6][75] - ML1
	 * 			Block Group ID [A0] - ML2
	 * 				Block ID [A1] - ML3
	 * 			Simple Block ID [A3] - ML2
	 */
//	cout<<m->header[0]<<m->header[1]<<m->header[2]<<m->header[3]<<" "<<m->mp4level<<endl;
	
	if (m->mp4level==ML0)
	{
		if(m->header[0]=='f' && m->header[1]=='t' && m->header[2]=='y' && m->header[3]=='p' )
			m->li.id0 = FTYP;
		else if(m->header[0]=='m' && m->header[1]=='o' && m->header[2]=='o' && m->header[3]=='v' )
			m->li.id0 = MOOV;
		else if(m->header[0]=='s' && m->header[1]=='i' && m->header[2]=='d' && m->header[3]=='x')
			m->li.id0 = SIDX;
		else if(m->header[0]=='m' && m->header[1]=='o' && m->header[2]=='o' && m->header[3]=='f')
			m->li.id0 = MOOF;
		else if(m->header[0]=='m' && m->header[1]=='d' && m->header[2]=='a' && m->header[3]=='t')
			m->li.id0 = MDAT;
		else
		{
#ifdef MP4DEBUG
			cout<<"UNKNOWN m->header "<<m->header[0]<<m->header[1]<<m->header[2]<<m->header[3]<<endl;
#endif
			m->li.id0 = MUNKNOWNL0;
		}
	}
	else if(m->mp4level==ML1)
	{
		if(m->header[0]=='m' && m->header[1]=='v' && m->header[2]=='h' && m->header[3]=='d')
			m->li.id1 = MVHD;
		else if (m->header[0]=='t' && m->header[1]=='r' && m->header[2]=='a' && (m->header[3]=='k' || m->header[3]=='f'))
			m->li.id1 = TRAK;
        else if(m->header[0]=='m' && m->header[1]=='v' && m->header[2]=='e' && m->header[3]=='x')
            m->li.id1 = MVEX;
		else
        {
#ifdef MP4DEBUG
            cout<<"UNKNOWN1 m->header "<<m->header[0]<<m->header[1]<<m->header[2]<<m->header[3]<<endl;
#endif
			m->li.id1 = MUNKNOWNL1;
        }
	}
	else if(m->mp4level==ML2)
	{
		if(m->li.id1 == TRAK)
		{
			if(m->header[0]=='m' && m->header[1]=='d' && m->header[2]=='i' && m->header[3]=='a')
				m->li.id2 = MDIA;
			else if(m->header[0]=='t' && m->header[1]=='k'  && m->header[2]=='h' && m->header[3]=='d')
				m->li.id2 = TKHD;
			else if(m->header[0]=='t' && m->header[1]=='f'  && m->header[2]=='h' && m->header[3]=='d')
				m->li.id2 = TFHD;
			else if(m->header[0]=='t' && m->header[1]=='f'  && m->header[2]=='d' && m->header[3]=='t')
				m->li.id2 = TFDT;
            else if (m->header[0]=='t' && m->header[1]=='r' && m->header[2]=='u' && m->header[3]=='n')
                m->li.id2 = TRUN;
			else
            {
#ifdef MP4DEBUG
                cout<<"UNKNOWN2 m->header "<<m->header[0]<<m->header[1]<<m->header[2]<<m->header[3]<<endl;
#endif
				m->li.id2 = MUNKNOWNL2;
            }

		}
		else if(m->li.id1 == MVEX)
        {
            if (m->header[0]=='t' && m->header[1]=='r' && m->header[2]=='e' && m->header[3]=='x')
                m->li.id2 = TREX;
            else if (m->header[0]=='m' && m->header[1]=='e' && m->header[2]=='h' && m->header[3]=='d')
                m->li.id2 = MEHD;
            else
            {
#ifdef MP4DEBUG
                cout<<"UNKNOWN2 m->header "<<m->header[0]<<m->header[1]<<m->header[2]<<m->header[3]<<endl;
#endif
                m->li.id2 = MUNKNOWNL2;
            }

        }
        else
			m->li.id2 = MUNKNOWNL2;
	}
	else if (m->mp4level==ML3)
	{
		if(m->li.id2 == MDIA)
		{
			if(m->header[0]=='m' && m->header[1]=='i' && m->header[2]=='n' && m->header[3]=='f')
				m->li.id3 = MINF;

			else if(m->header[0]=='m' && m->header[1]=='d' && m->header[2]=='h' && m->header[3]=='d')
				m->li.id3 = MDHD;
			else if(m->header[0]=='h' && m->header[1]=='d' && m->header[2]=='l' && m->header[3]=='r')
				m->li.id3 = HDLR;
			else
				m->li.id3 = MUNKNOWNL3;
		}

	}
	else if (m->mp4level==ML4)
	{
		if(m->li.id3==MINF)
		{
			if(m->header[0]=='s' && m->header[1]=='t' && m->header[2]=='b' && m->header[3]=='l')
				m->li.id4 = STBL;
			else
				m->li.id4 = MUNKNOWNL4;
		}
	}
	else if (m->mp4level==ML5)
	{

		if(m->li.id4 == STBL)
		{

			if(m->header[0]=='s' && m->header[1]=='t' && m->header[2]=='t' && m->header[3]=='s')
				m->li.id5 = STTS;
			else if(m->header[0]=='s' && m->header[1]=='t' && m->header[2]=='s' && m->header[3]=='z')
				m->li.id5 = STSZ;

			else if(m->header[0]=='s' && m->header[1]=='t' && m->header[2]=='s' && m->header[3]=='s')
				m->li.id5 = STSS;
			else if(m->header[0]=='s' && m->header[1]=='t' && m->header[2]=='s' && m->header[3]=='c')
				m->li.id5 = STSC;
			else if(m->header[0]=='s' && m->header[1]=='t' && m->header[2]=='c' && m->header[3]=='o' )
				m->li.id5 = STCO;
			else if(m->header[0]=='c' && m->header[1]=='o' && m->header[2]=='6' && m->header[3]=='4' )
				m->li.id5 = CO64;

			else
				m->li.id5 = MUNKNOWNL5;
		}
	}
	else
	{
#ifdef DEBUG
		printf("The level is not in permissable range\n");
#endif
    	return false;
	}
	//printf("ID:[%x%x%x%x]\t",m->header[0],m->header[1], m->header[2], m->header[3]);
	return true;
}

/* return : the number of bytes that were read by the function.
 *
 */
int mp4_get_body (unsigned char * stream, int len, uint64_t size, struct mp4_i * m)
{
	int ret=-1;
	m->mp4state=MREADSIZE;
//	cout<<"checking body "<<m->li.id0<<endl;

	switch(m->mp4level)
	{
		case ML0:
			switch (m->li.id0)
			{
				case MOOV:
	#ifdef MP4DEBUG
					printf("MOOV:%ld\n",m->li.size0);
	#endif
					m->mp4level = ML1;
					ret=0;
					break;
				case FTYP:
	#ifdef MP4DEBUG
					printf("FTYP:%ld\n",m->li.size0);
	#endif
					m->mp4state = MREADFTYP;
					ret=0;
					break;
				case MOOF:
#ifdef MP4DEBUG
					printf("MOOF:%ld\n",m->li.size0);
#endif
					m->mp4level = ML1;
					ret=0;
					break;
				case MDAT:
#ifdef MP4DEBUG
					printf("MDAT:%ld\n",m->li.size0);
#endif
					//ret=m->li.size0;
					m->mp4state = MREADMDAT;
					ret=0;

					break;
				case SIDX:
#ifdef MP4DEBUG
					printf("SIDX:%ld\n",m->li.size0);
#endif
					m->mp4state = MREADSIDX;
					ret=0;
					break;
				case MUNKNOWNL0:
#ifdef MP4DEBUG
					printf("MUNKNOWN:%ld\n",m->li.size0);
#endif
					ret=m->li.size0;
					break;
				default:
					ret= -1;
					break;
			}
			break;
		case ML1:
			m->li.size0-=m->li.size1;
			switch (m->li.id1)
			{
				case TRAK:
#ifdef MP4DEBUG
					printf("\tTRAK:%ld\n",m->li.size1);
#endif
					m->mp4level = ML2;
					ret= 0;
					break;
                case MVHD:
                    m->mp4state=MREADMVHD;
#ifdef MP4DEBUG
                    printf("\tMVHD:%ld\n",m->li.size1);
#endif
                    ret= 0;
                    break;
                case MVEX:
#ifdef MP4DEBUG
                    printf("\MVEX:%ld\n",m->li.size1);
#endif
                    m->mp4level = ML2;
                    ret= 0;
                    break;
				case MUNKNOWNL1:
#ifdef MP4DEBUG
					printf("\tMUNKNOWNL1:%ld\n",m->li.size1);
#endif
					ret= m->li.size1;
					break;
				default:
#ifdef MP4DEBUG
					printf("\tDEFAULTL1:%ld\n",m->li.size1);
#endif
					ret= -1;
					break;
			}
			break;
		case ML2:
			m->li.size1-=m->li.size2;
			switch (m->li.id2)
			{
				case MDIA:
#ifdef MP4DEBUG
					printf("\t\tMDIA:%ld\n",m->li.size2);
#endif
					m->mp4level=ML3;
					//printf("BlockGroup : %x%x%x\n", stream[0],stream[1],stream[2]);
					ret= 0;
					break;
				case TKHD:
#ifdef MP4DEBUG
					printf("\t\tTKHD:%ld\n",m->li.size2);
#endif
					m->mp4state=MREADTKHD;
					ret= 0;
					break;
				case TFHD:
#ifdef MP4DEBUG
					printf("\t\tTFHD:%ld\n",m->li.size2);
#endif
					m->mp4state=MREADTFHD;
					ret= 0;
					break;
				case TFDT:
#ifdef MP4DEBUG
					printf("\t\tTFDT:%ld\n",m->li.size2);
#endif
					m->mp4state=MREADTFDT;
					ret= 0;
					break;
                case TRUN:
#ifdef MP4DEBUG
                    printf("\t\tTRUN:%ld\n",m->li.size2);
#endif
                    m->mp4state=MREADTRUN;
                    ret= 0;
                    break;
                case TREX:
#ifdef MP4DEBUG
                    printf("\t\tTREX:%ld\n",m->li.size2);
#endif
                    m->mp4state=MREADTREX;
                    ret= 0;
                    break;
                case MEHD:
#ifdef MP4DEBUG
                    printf("\t\tMEHD:%ld\n",m->li.size2);
#endif
                    m->mp4state=MREADMEHD;
                    ret= 0;
                    break;
				case MUNKNOWNL2:
#ifdef MP4DEBUG
					printf("\t\tMUNKNOWNL2:%ld\n",m->li.size2);
#endif
					ret= m->li.size2;
					break;
				default:
#ifdef MP4DEBUG
					printf("\tDEFAULTL2:%ld\n",m->li.size1);
#endif
					ret= -1;
					break;
			}
			break;
		case ML3:
			m->li.size2-=m->li.size3;
			switch(m->li.id3)
			{
				case MINF:
#ifdef MP4DEBUG
					printf("\t\tMINF:%ld\n",m->li.size3);
#endif
					m->mp4level=ML4;
					//printf("BlockGroup : %x%x%x\n", stream[0],stream[1],stream[2]);
					ret= 0;
					break;
				case MDHD:
#ifdef MP4DEBUG
					printf("\t\t\t\tMDHD:%ld\n",m->li.size3);
#endif
					m->mp4state=MREADMDHD;
					ret= 0;
					break;
				case HDLR:
#ifdef MP4DEBUG
					printf("\t\tHDLR:%ld\n",m->li.size2);
#endif
					m->mp4state=MREADHDLR;
					ret= 0;
					break;
				case MUNKNOWNL3:
#ifdef MP4DEBUG
					printf("\t\t\tMUNKNOWNL3:%ld\n",m->li.size3);
#endif
					ret= m->li.size3;
					break;
				default:
#ifdef MP4DEBUG
					printf("\tDEFAULTL3:%ld\n",m->li.size1);
#endif
					ret=-1;
					break;
			}
			break;
		case ML4:
			m->li.size3-=m->li.size4;
			switch(m->li.id4)
			{

				case STBL:
#ifdef MP4DEBUG
					printf("\t\tSTBL:%ld\n",m->li.size4);
#endif
					m->mp4level = ML5;
					ret= 0;
					break;
				case MUNKNOWNL4:
#ifdef MP4DEBUG
					printf("\t\t\tMUNKNOWNL4:%ld\n",m->li.size4);
#endif
					ret= m->li.size4;
					break;
				default:
#ifdef MP4DEBUG
					printf("\tDEFAULTL4:%ld\n",m->li.size1);
#endif
					ret=-1;
					break;
			}
			break;
		case ML5:
			m->li.size4-=m->li.size5;
			switch(m->li.id5)
			{

				case STTS:
#ifdef MP4DEBUG
					printf("\t\t\t\tSTTS:%ld\n",m->li.size5);
#endif
					m->mp4state=MREADSTTS;
					ret= 0;
					break;
				case STSZ:
#ifdef MP4DEBUG
					printf("\t\t\t\tSTSZ:%ld\n",m->li.size5);
#endif
					m->mp4state=MREADSTSZ;
					ret= 0;
					break;
				case STSC:
#ifdef MP4DEBUG
					printf("\t\t\t\tSTSC:%ld\n",m->li.size5);
#endif
					m->mp4state=MREADSTSC;
					ret= 0;
					break;
				case STCO:
				case CO64:
#ifdef MP4DEBUG
					printf("\t\t\t\tSTCO:%ld\n",m->li.size5);
#endif
					m->mp4state=MREADSTCO;
					ret= 0;
					break;
				case STSS:
#ifdef MP4DEBUG
					printf("\t\t\t\tSTSS:%ld\n",m->li.size5);
#endif
					m->mp4state=MREADSTSS;
					ret= 0;
					break;

				case MUNKNOWNL5:
#ifdef MP4DEBUG
					printf("\t\t\tMUNKNOWNL5:%ld\n",m->li.size5);
#endif
					ret= m->li.size5;
					break;

				default:
#ifdef MP4DEBUG
					printf("\tDEFAULTL5:%ld\n",m->li.size1);
#endif
					ret=-1;
					break;
			}
			break;

		default:
#ifdef MP4DEBUG
					printf("\tDEFAULTDEFAULT:%ld\n",m->li.size1);
#endif
			ret= -1;
			break;

	}
	checkmp4level(m);
//	printf("Return from ReadBODY %d \n", ret);
	if(ret<0)
		exit(0);
	//whata++;
	//if(whata>20)
		//exit(0);
	return ret;
}

void checkmp4level(struct mp4_i * m)
{
	if(m->mp4level==ML5 && m->li.size4==0)
		m->mp4level=ML4;
	if(m->mp4level==ML4 && m->li.size3==0)
		m->mp4level=ML3;
	if(m->mp4level==ML3 && m->li.size2==0)
		m->mp4level=ML2;
	if(m->mp4level==ML2 && m->li.size1==0)
		m->mp4level=ML1;
	if(m->mp4level==ML1 && m->li.size0==0)
		m->mp4level=ML0;
}

int mp4_read_mvhd(unsigned char * stream, int len, struct mp4_i * m)
{
//	static unsigned char m->header[SIZEMVHD] = {0};
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;

	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=SIZEMVHD;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		m->li.size1-=ret;

		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	m->mp4tinfo.timescale = atouint32 ((char *)m->header+12, endianness);
	m->mp4tinfo.duration = atouint32 ((char *)m->header+16, endianness);
	float tmpdur = (float)m->mp4tinfo.duration/(float)m->mp4tinfo.timescale;
	if(metric.duration< tmpdur)
		metric.duration =tmpdur;
	m->mp4tinfo.prefrate = atouint32 ((char *)m->header+20, endianness);
#ifdef MP4DEBUG
	cout<<"MVHD Data is : "<<m->mp4tinfo.timescale<<" "<<m->mp4tinfo.duration<<" "<<m->mp4tinfo.prefrate<<endl;
#endif
	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	ret = m->li.size1;
	m->li.size1=0;

#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}

int mp4_read_mdhd(unsigned char * stream, int len, struct mp4_i * m)
{
//	static unsigned char m->header[SIZEMDHD] = {0};
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=SIZEMDHD;
		m->header = new unsigned char [m->tocopy];

	}
	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		m->li.size3-=ret;
		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	m->tracks[m->currtrakid].duration=atouint32 ((char *)m->header+16, endianness);
	m->tracks[m->currtrakid].timescale= atouint32 ((char *)m->header+12, endianness);

//	cout<<"MDHD Data is : "<<m->tracks[m->currtrakid].m->timescale<<" "<<m->tracks[m->currtrakid].duration<<endl;
	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	ret = m->li.size3;
	m->li.size3=0;

#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}


int mp4_read_tkhd(unsigned char * stream, int len, struct mp4_i * m)
{
//	static unsigned char m->header[SIZETKHD] = {0};
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=SIZETKHD;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		m->li.size2-=ret;
		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	m->currtrakid = atouint32 ((char *)m->header+12, endianness);
	m->tracks[m->currtrakid].duration=0;
	m->tracks[m->currtrakid].timescale=0;
	m->tracks[m->currtrakid].stable = NULL;
#ifdef MP4DEBUG
	cout<<"TKHD Data is : "<<m->currtrakid<<endl;
#endif
	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	ret = m->li.size2;
	m->li.size2=0;


#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}


int mp4_read_stss(unsigned char * stream, int len, struct mp4_i * m)
{
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=m->li.size5;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		m->li.size5-=ret;
		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	m->tracks[m->currtrakid].keyframes = atouint32 ((char *)m->header+4, endianness);

	cout<<"Number of keyframes : "<<m->tracks[m->currtrakid].keyframes<<endl;
	if(m->tracks[m->currtrakid].stable!=NULL)
	{
		for(unsigned int i=0; i<m->tracks[m->currtrakid].keyframes; i++)
		{

			uint32_t j = atouint32 ((char *)m->header+8+(i*4), endianness);
	//		cout<<j<<endl;
			m->tracks[m->currtrakid].stable[j-1].key=true;
		}
	}
	else
		cout<<"Keyframes are not marked!! \n";
	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL; 	ret = m->li.size5;
	m->li.size5=0;
//	exit(0);

#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}

/*time to sample, duration of the samples*/
int mp4_read_stts(unsigned char * stream, int len, struct mp4_i * m)
{
//	static uint32_t entries=0;
	unsigned char * tmp;
//	static unsigned char * m->table=NULL;
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=SIZESTTS;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	tmp = stream;
	do{
		if (len<m->tocopy)
		{
			if(m->table==NULL)
				memcpy(m->header+m->index, stream, len);
			else
				memcpy(m->table+m->index, tmp, len);
			m->index+=len;
			m->tocopy-=len;
			m->li.size5-=ret;
			//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
			return ret;
		}

		if(m->table==NULL)
		{
			memcpy(m->header+m->index, stream, m->tocopy);
			m->entries = atouint32 ((char *)m->header+4, endianness);
#ifdef MP4DEBUG
			cout<<"entries in stts : "<<m->entries<<endl;
#endif
			m->index=0;
			tmp = stream+m->tocopy;
			len-=m->tocopy;
			m->tocopy =STTSENTRYSIZE*m->entries; /*each m->table entry has 8 bytes. 4 for number of samples, and 4 for sample duration*/
			m->table = new unsigned char [m->tocopy];
		}
		else
		{
			memcpy(m->table+m->index, tmp, m->tocopy);
			break;
		}
	}while(1);
	/*The whole datasize has been copied to m->header*/
	uint32_t tmpsize=0;/*counts the total number of samples*/
	for(unsigned int i =0; i<m->entries; i++)
		tmpsize+= atouint32 ((char *)m->table+(i*STTSENTRYSIZE), endianness);
	if(m->tracks[m->currtrakid].stable==NULL)
	{
		m->tracks[m->currtrakid].entries = tmpsize;
		m->tracks[m->currtrakid].stable = new tts [tmpsize];
		memzero(m->tracks[m->currtrakid].stable, tmpsize*sizeof(tts));
	}
	else if(m->tracks[m->currtrakid].entries!=tmpsize)
	{
		cout<<"Conflict in number of entries\n";
		return -1;
	}
	uint32_t count=0, lastcount=0;
	double timegone = 0;
	for(unsigned int i=0,j=0; i<m->entries; i++)
	{
		count = atouint32 ((char *)m->table+(i*STTSENTRYSIZE), endianness);
		for(; j<count+lastcount; j++)
		{
			m->tracks[m->currtrakid].stable[j].stime = timegone;
			m->tracks[m->currtrakid].stable[j].sdur = atouint32 ((char *)m->table+(i*STTSENTRYSIZE+STTSSAMPLENUMSIZE), endianness);
			timegone+=((double)m->tracks[m->currtrakid].stable[j].sdur*1000/(double)m->tracks[m->currtrakid].timescale);
		}
#ifdef MP4DEBUG
		cout<<"STTS Data is : "<<count<<" "<<m->tracks[m->currtrakid].stable[i].sdur<<endl;
#endif
		lastcount = count;
	}

	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	delete [] m->table;
	m->table = NULL;
	m->sidx = NULL;
	ret = m->li.size5;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	m->li.size3=0;
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}

/*You use sample size atoms to specify the size of each sample in the media (in bytes). Sample size atoms have an atom type of 'stsz'.
The sample size atom contains the sample count and a m->table giving the size of each sample.
This allows the media data itself to be unframed. The total number of samples in the media is always indicated in the sample count.
If the default size is indicated, then no m->table follows.
*/


int mp4_read_stsz(unsigned char * stream, int len, struct mp4_i * m)
{
	unsigned char * tmp;
//	static unsigned char * m->table=NULL;
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	static uint32_t tmpsize=0;
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=SIZESTSZ;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	tmp = stream;
	do{
		if (len<m->tocopy)
		{
			if(m->table==NULL)
				memcpy(m->header+m->index, stream, len);
			else
				memcpy(m->table+m->index, tmp, len);
			m->index+=len;
			m->tocopy-=len;
			m->li.size5-=ret;
			//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
			return ret;
		}

		if(m->table==NULL)
		{
			memcpy(m->header+m->index, tmp, m->tocopy);
			tmpsize = atouint32 ((char *)m->header+4, endianness);
		//	cout<<"STSZ Data is : "<<tmpsize<<endl;
			m->entries = atouint32 ((char *)m->header+8, endianness); /*indicates count of samples*/
			if(tmpsize>0) /* then no m->table follows, all samples are of same size*/
				break;
		//	cout<<"m->entries in stsz : "<<m->entries<<" "<<m->tocopy<<endl;
			m->index=0;
			//tmp = stream;
	//		printf("%x%x%x%x\n",tmp[0],tmp[1],tmp[2],tmp[3]);
			tmp = tmp+m->tocopy;
			len-=m->tocopy;

		//	printf("%x%x%x%x\n",tmp[0],tmp[1],tmp[2],tmp[3]);
			m->tocopy = 4*m->entries; /*each m->table entry has 4 bytes for sample size*/
			m->table = new unsigned char [m->tocopy];
		}
		else
		{
			memcpy(m->table+m->index, tmp, m->tocopy);
			break;
		}
	}while(1);
	/*The whole datasize has been copied to m->header*/
	if(m->tracks[m->currtrakid].stable==NULL)
	{
		m->tracks[m->currtrakid].entries = m->entries;
		m->tracks[m->currtrakid].stable = new tts [m->entries];
		memzero(m->tracks[m->currtrakid].stable, m->entries*sizeof(tts));
	}
	if(tmpsize>0)
		for(uint i =0; i<m->entries; i++)
		{
			m->tracks[m->currtrakid].stable[i].size = tmpsize;
		//	cout<<"STSZ Data is : "<<tmpsize<<" "<<m->tracks[m->currtrakid].stable[i].sdur<<" "<<m->tracks[m->currtrakid].stable[i].size<<" "<<m->tracks[m->currtrakid].stable[i].chunk<<" "<<i<<endl;

		}
	else
		for(uint i =0; i<m->entries; i++)
		{
			m->tracks[m->currtrakid].stable[i].size = atouint32 ((char *)m->table+(i*4), endianness);
//			cout<<"STSZ Data is through m->entries: "<<m->tracks[m->currtrakid].stable[i].sdur<<" "<<m->tracks[m->currtrakid].stable[i].size<<" "<<m->tracks[m->currtrakid].stable[i].chunk<<" "<<i<<endl;

		}
	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	delete [] m->table;
	m->table = NULL;
	ret = m->li.size5;
	tmpsize=0;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	m->li.size5=0;
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}
/*
As samples are added to a media, they are collected into chunks that allow optimized data access. A chunk contains one or more samples.
Chunks in a media may have different sizes, and the samples within a chunk may have different sizes.
The sample-to-chunk atom stores chunk information for the samples in a media.
Sample-to-chunk atoms have an atom type of 'stsc'. The sample-to-chunk atom contains a m->table that maps samples
to chunks in the media data stream. By examining the sample-to-chunk atom, you can determine the chunk that contains a specific sample.
*/
int mp4_read_stsc(unsigned char * stream, int len, struct mp4_i * m)
{
	unsigned char * tmp;
//	static unsigned char * m->table=NULL;
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=8;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	tmp = stream;
	do{
		if (len<m->tocopy)
		{
			if(m->table==NULL)
				memcpy(m->header+m->index, stream, len);
			else
				memcpy(m->table+m->index, tmp, len);
			m->index+=len;
			m->tocopy-=len;
			m->li.size5-=ret;
			//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
			return ret;
		}

		if(m->table==NULL)
		{
			memcpy(m->header+m->index, stream, m->tocopy);
			m->entries = atouint32 ((char *)m->header+4, endianness);
		//	cout<<"m->entries in stsc : "<<m->entries<<endl;
			m->index=0;
			tmp = stream+m->tocopy;
			len-=m->tocopy;
			m->tocopy = 12*m->entries; /*each m->table entry has 12 bytes. 4 for first chunk, 4 for number of samples/chunk, and 4 for sample description*/
			m->table = new unsigned char [m->tocopy];
		}
		else
		{
			memcpy(m->table+m->index, tmp, m->tocopy);
			break;
		}
	}while(1);
	/*The whole datasize has been copied to m->header*/
	if(m->tracks[m->currtrakid].stable==NULL)
	{
		cout<<"Reading Sample-to-chunk and the sample m->table is still uninitialized!\n";
		return -1;
	}
	uint32_t spc=0, firstchunk=0, lastchunk=0, chunk=0;
	unsigned int curr_sample=0, j;

	for(unsigned int c=0; c<m->entries; c++)
	{
		spc= atouint32 ((char *)m->table+(c*12+4), endianness);

		if(c<m->entries)
			firstchunk= atouint32 ((char *)m->table+(c*12), endianness);
		if(c+1<m->entries)
			lastchunk = atouint32 ((char *)m->table+((c+1)*12), endianness);
		else
			lastchunk = firstchunk+((m->tracks[m->currtrakid].entries-curr_sample)/spc);
		chunk = firstchunk;
		while(chunk<lastchunk)
		{
			if(curr_sample>=m->tracks[m->currtrakid].entries)
			{
//				int i = j;
//				cout<<curr_sample<<" "<<m->tracks[m->currtrakid].m->entries<<endl;
//				//cout<<"STSC Data is : "<<m->tracks[m->currtrakid].stable[i].sdur<<" "<<m->tracks[m->currtrakid].stable[i].size<<" "<<m->tracks[m->currtrakid].stable[i].chunk<<" "<<i<<endl;
				cout<<"Reading Sample-to-chunk and the samples exceeded sample m->table m->entries!\n";
				return -1;
			}
			for(j=curr_sample; j<spc+curr_sample; j++)
			{
				m->tracks[m->currtrakid].stable[j].chunk = chunk;
//				int i = j;
//				cout<<"STSC Data is : "<<m->tracks[m->currtrakid].stable[i].sdur<<" "<<m->tracks[m->currtrakid].stable[i].size<<" "<<m->tracks[m->currtrakid].stable[i].chunk<<" "<<i<<endl;
//				cout<<firstchunk<<" "<<lastchunk<<" "<<spc<<endl;
			}
			++chunk;
			curr_sample = j;
		}
	}

	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL; 	delete [] m->table;
	m->table = NULL;
	ret = m->li.size5;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	m->li.size5=0;
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}

/*
 * Chunk offset atoms identify the location of each chunk of data in the mediaÕs data stream. Chunk offset atoms have an atom type of 'stco'.
The chunk-offset m->table gives the m->index of each chunk into the containing file. There are two variants, permitting the use of 32-bit or 64-bit offsets.
The latter is useful when managing very large movies. Only one of these variants occurs in any single instance of a sample m->table atom.
Note that offsets are file offsets, not the offset into any atom within the file (for example, a 'mdat' atom).
This permits referring to media data in files without any atom structure. However, be careful when constructing a self-contained
QuickTime file with its metadata (movie atom) at the front because the size of the movie atom affects the chunk offsets to the media data.

 */


int mp4_read_stco(unsigned char * stream, int len, struct mp4_i * m)
{
	unsigned char * tmp;
//	static unsigned char * m->table=NULL;
//	static int m->index =0; /* The write position inside m->header */
//	static int m->tocopy = -1; /* number of bytes of the MP4 size that remain to be read from the data,
//                             -1 if not known yet. */
	int ret =len;

	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->tocopy=SIZESTCO;
		m->header = new unsigned char [m->tocopy];
	}
	/*if m->header is incomplete*/
	tmp = stream;
	do{
		if (len<m->tocopy)
		{
			if(m->table==NULL)
				memcpy(m->header+m->index, stream, len);
			else
				memcpy(m->table+m->index, tmp, len);
			m->index+=len;
			m->tocopy-=len;
			m->li.size5-=ret;
			//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
			return ret;
		}

		if(m->table==NULL)
		{
			memcpy(m->header+m->index, stream, m->tocopy);
			m->entries = atouint32 ((char *)m->header+4, endianness);
		//	cout<<"m->m->entries in stco : "<<m->m->entries<<endl;
			m->index=0;
			tmp = stream+m->tocopy;
			len-=m->tocopy;
			if(m->li.id5==STCO)
			{
				m->tocopy = 4*m->entries; /*each m->table entry has 4 bytes if it's stco or 8 bytes if it's co64*/
				m->table = new unsigned char [m->tocopy];
			}
			else
			{
				m->tocopy = 8*m->entries; /*each m->table entry has 4 bytes if it's stco or 8 bytes if it's co64*/
				m->table = new unsigned char [m->tocopy];
			}
		}
		else
		{
			memcpy(m->table+m->index, tmp, m->tocopy);
			break;
		}
	}while(1);
	/*The whole datasize has been copied to m->header*/
	if(m->tracks[m->currtrakid].stable==NULL)
	{
		cout<<"Reading Sample-to-chunk and the sample m->table is still uninitialized!\n";
		return -1;
	}
	uint64_t co;

	int so = 0 ; /* sample offset, used to store the size of last sample */
	unsigned int i=0;
	for(unsigned int c=1; c<=m->entries; c++)
	{
		if(m->li.id5==STCO)
			co = atouint32 ((char *)m->table+((c-1)*4), endianness);
		else
			co = atouint64 ((char *)m->table+((c-1)*8), endianness);
	//	cout<<co<<endl;
		while(i< m->tracks[m->currtrakid].entries && m->tracks[m->currtrakid].stable[i].chunk==c)
		{
			so += m->tracks[m->currtrakid].stable[i].size; /*add the size of the sample to it*/
			m->tracks[m->currtrakid].stable[i].offset = co + so; /*this now is the last byte of the sample*/
	//		cout<<"STCO Data is : "<<m->tracks[m->currtrakid].stable[i].stime<<" "<<m->tracks[m->currtrakid].stable[i].sdur<<" "<<m->tracks[m->currtrakid].stable[i].size<<" "<<m->tracks[m->currtrakid].stable[i].chunk<<" "<<m->tracks[m->currtrakid].stable[i].offset<<" "<<i<<endl;
			++i;
		}
		so = 0;

	}
//	cout<<"things are all peachy\n";  fflush(stdout);

	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	delete [] m->table;
//	cout<<"just peachy\n";  fflush(stdout);

	m->table = NULL;
	ret = m->li.size5;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	m->li.size5=0;
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}

int mp4_read_mdat(unsigned char * stream, int len, struct mp4_i * m)
{
	if (m->samplemap.size()==0)
	{
//		long start = gettimelong();
	//	cout<<"iterating the m->tracks\n";
		for( map<uint32_t, struct tinfo>::iterator ii=m->tracks.begin(); ii!=m->tracks.end(); ++ii)
		{
			if((*ii).second.stable== NULL)
			{
				cout<<"Received mdat and the sample m->table is still empty for one of the m->tracks "<<(*ii).first<<endl;
				return -1;
			}
			for(unsigned int i=0; i< (*ii).second.entries; i++)
			{

				double j = (*ii).second.stable[i].stime;
			//	cout<<"adding value for "<<(*ii).first<<" "<<i<<endl;
				if(m->samplemap[j] < (*ii).second.stable[i].offset)
					m->samplemap[j] = (*ii).second.stable[i].offset;
				if(Mp4Model)
				{
					cout<<"youtubeevent13\t"<<(*ii).second.handler<<"\t"<<(*ii).first<<"\t"<<(*ii).second.stable[i].stime<<"\t"<<(*ii).second.stable[i].chunk<<"\t"\
					<<(*ii).second.stable[i].size<<"\t"<< (*ii).second.stable[i].sdur<<"\t"<<(*ii).second.stable[i].offset<<"\t"<<(*ii).second.stable[i].key<<endl;

				}	//	writelog(tmp, strlen(tmp));

			}
			delete [] (*ii).second.stable;
			(*ii).second.stable = NULL;
			if(Mp4Model)
			{
				cout<<"youtubeevent15\t"<<(*ii).second.handler<<"\t"<< (*ii).second.duration;
				cout<<"\t"<< (*ii).second.entries;
				cout<<"\t"<< (*ii).second.keyframes;
				cout<<"\t"<< (*ii).second.timescale<<endl;
			}
#ifdef MP4DEBUG
			cout<<"table is here "<<(*ii).first<<" of size "<< (*ii).second.entries<<endl;
			cout<<"Duration "<< (*ii).second.duration<<endl;
			cout<<"Entries "<< (*ii).second.entries<<endl;
			cout<<"Keyframes "<< (*ii).second.keyframes<<endl;
			cout<<"Timescale "<< (*ii).second.timescale<<endl;
#endif
		}

		if(Mp4Model)
		{
//			   for( map<double, uint64_t>::iterator jj=m->samplemap.begin(); jj!=m->samplemap.end(); ++jj)
//				   cout<<"youtubeevent14\t"<<(*jj).first<<"\t"<<(*jj).second<<endl;
			exit(0);
		}
//		long end = gettimelong();
//		cout<<"It took "<<end-start<<" microseconds\n";
	}
//	cout<<"read data now for map size "<<m->samplemap.size()<<endl;

	   for( map<double, uint64_t>::iterator ii=m->samplemap.begin(); ii!=m->samplemap.end();)
	   {
	//       cout << (*ii).first << ": " << (*ii).second << endl;
		//   cout<<m->rxbytesmp4<<" "<<(*ii).second<<endl;
	       if(m->rxbytesmp4>(*ii).second)
	       {
		       metric.TSnow = (*ii).first;
	    	   m->samplemap.erase(ii++);
	       }
	       else
	    	   break;
	   }


	m->li.size0-=len;
	checkmp4level(m);
	return len;

}
