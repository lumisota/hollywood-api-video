/*
 * mpeg_a.cpp
 *
 *  Created on: Mar 5, 2014
 *      Author: sahsan
 */


#include "mpeg.h"

extern bool endianness;
extern metrics metric;
extern bool Mp4Model;


/*getting information about the subsegments/fragments*/
int mp4_read_sidx(unsigned char * stream, int len, struct mp4_i * m)
{
	/*Ref is a subsegment, refct is the Reference count, or in other words the number of fragments/subsegments*/
	uint16_t refct=0;
	unsigned char * tmp;
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header!=NULL)
	{
		if(SIZESIDX>m->li.size0)
		{
			fprintf(stderr, "mp4_read_sidx: sidx has less bytes than the sidx header size, header: %d, size %lu\n",SIZESIDX,m->li.size0);
			return -1;
		}
		m->header = new unsigned char [SIZESIDX];
		m->tocopy=SIZESIDX;
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
			m->li.size0-=ret;
			//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
			return ret;
		}

		if(m->table==NULL)
		{
			memcpy(m->header+m->index, stream, m->tocopy);
//			refid = atouint32 ((char *)m->header+4, endianness);
			m->timescale = atouint32 ((char *)m->header+8, endianness);
//			ept  = atouint32 ((char *)m->header+12, endianness);
//			firstoffset  = atouint32 ((char *)m->header+16, endianness);
			refct = atouint16 ((char *)m->header+22, endianness);
//			refsize  = atouint32 ((char *)m->header+24, endianness);
//			ssdur  = atouint32 ((char *)m->header+28, endianness);
//			sapdelta  = atouint32 ((char *)m->header+32, endianness);

//			cout<<"RefID "<<refid<<endl;
//			cout<<"Timescale "<<m->timescale<<endl;
//			cout<<"Ept "<<ept<<endl;
//			cout<<"Firstoffset "<<firstoffset<<endl;
			//cout<<"Refct "<<refct<<endl;
//			cout<<"RefSize "<<refsize<<"\t";
//			cout<<"SSdur "<<ssdur<<"\t";
//			cout<<"sapDelta "<<sapdelta<<endl;
//			cout<<"RefSize "<<atouint32 ((char *)m->header+36, endianness)<<"\t";
//			cout<<"SSdur "<<atouint32 ((char *)m->header+40, endianness)<<"\t";
//			cout<<"sapDelta "<<atouint32 ((char *)m->header+44, endianness)<<endl;
//			cout<<"RefSize "<<atouint32 ((char *)m->header+48, endianness)<<"\t";
//			cout<<"SSdur "<<atouint32 ((char *)m->header+52, endianness)<<"\t";
//			cout<<"sapDelta "<<(atouint32 ((char *)m->header+56, endianness) & 0x0FFFFFFF)<<endl;
//			cout<<"entries in sidx : "<<refct<<endl;
			m->index=0;
			tmp = stream+m->tocopy;
			len-=m->tocopy;
			m->tocopy =SIDXENTRYSIZE*refct; /*each sidx entry has 16 bytes. 4 for refsize, and 4 for sample duration, 4 for sap delta time*/
			m->table = new unsigned char [m->tocopy];
//			cout<<m->tocopy<<endl;
		}
		else
		{
			memcpy(m->table+m->index, tmp, m->tocopy);
			break;
		}
	}while(1);
	/*The whole datasize has been copied to m->header*/
	m->ss = new struct subsegs [refct];
//				cout<<"RefSize "<<atouint32 ((char *)m->table, endianness)<<"\t";
//				cout<<"SSdur "<<atouint32 ((char *)m->table+4, endianness)<<"\t";
//				cout<<"sapDelta "<<atouint32 ((char *)m->table+8, endianness)<<endl;
//				cout<<"RefSize "<<atouint32 ((char *)m->table+12, endianness)<<"\t";
//				cout<<"SSdur "<<atouint32 ((char *)m->table+16, endianness)<<"\t";
//				cout<<"sapDelta "<<(atouint32 ((char *)m->table+20, endianness) & 0x0FFFFFFF)<<endl;
	double timegone=0;
//	cout<<"here "<<refct<<endl;
	for(int i=0; i<refct; i++)
	{
		int offset = (i*SIDXENTRYSIZE);
		m->ss[i].refsize = atouint32 ((char *)m->table+offset, endianness);
		m->ss[i].ssdur = atouint32 ((char *)(m->table+offset+SIDXSINGLESIZE), endianness);
		m->ss[i].sapdelta = atouint32 ((char *)(m->table+offset+(2*SIDXSINGLESIZE)), endianness);
#ifdef MP4DEBUGA
		if(strcmp(m->tracks[m->currtrakid].handler, "vide")==0)
		{
			timegone+=m->ss[i].ssdur/m->timescale;
	//		cout<<"m"<<metric.link+31<<"\t";
			cout<<m->tracks[m->currtrakid].handler<<"\t";
			cout<<offset+SIDXSINGLESIZE<<"\t";
			cout<<m->ss[i].refsize<<"\t";
			cout<<m->ss[i].ssdur<<"\t";
			cout<<m->ss[i].sapdelta<<"\t";
			cout<<m->timescale<<"\t";
			cout<<timegone<<"\t";
			cout<<m->ss[i].ssdur/m->timescale<<endl;
		}
		else
			cout<<m->tracks[m->currtrakid].handler<<endl<<endl;
#endif
	}
	m->tocopy = -1;
	m->index = 0;
	delete [] m->table;
	delete [] m->header;
	m->header = NULL;
	m->table = NULL;
	ret = m->li.size0;

	m->li.size0=0;
	checkmp4level(m);
	m->mp4state=MREADSIZE;
//	if(strcmp(m->tracks[m->currtrakid].handler, "vide")==0)
//		exit(0);
	return ret;


}



int mp4_read_ftyp(unsigned char * stream, int len, struct mp4_i * m)
{
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->header = new unsigned char [m->li.size0];
		m->tocopy= m->li.size0;
	}
	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		m->li.size0-=ret;
		//printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}

	memcpy(m->header+m->index, stream, m->tocopy);
	/*The whole datasize has been copied to m->header*/
	m->header[5]='\0';
	if(strstr((char *)m->header, "mp4")!=NULL)
	{
		m->ftyp = MP42;
	}
	else  if(strstr((char *)m->header, "dash")!=NULL)
	{
		m->ftyp = DASH;
	}
	else
	{
		m->ftyp = MUNKNOWNF;
		fprintf(stderr, "It is an unrecognized MP4 file format : %s", m->header);
		return -1;
	}

//	cout<<"File Type is "<<m->ftyp<<endl;

	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->header = NULL;
	ret = m->li.size0;
	m->li.size0=0;


#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}


int mp4_read_hdlr(unsigned char * stream, int len, struct mp4_i * m)
{
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->header = new unsigned char [m->li.size3];
		m->tocopy= m->li.size3;
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
	strncpy(m->tracks[m->currtrakid].handler, (char *)m->header+8, 4);
	m->tracks[m->currtrakid].handler[4]='\0';

//	cout<<"Handler is "<<m->tracks[m->currtrakid].handler<<" for track "<<m->currtrakid<<endl;
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

int mp4_read_tfdt(unsigned char * stream, int len, struct mp4_i * m)
{
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{

		m->header = new unsigned char [m->li.size2];
		m->tocopy= m->li.size2;
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

	if(atouint32 ((char *)m->header, endianness)==0)
		m->decodetime = atouint32 ((char *)m->header+4, endianness);
	else if ( m->li.size2 ==20)
		m->decodetime = atouint64 ((char *)m->header+4, endianness);
	else
	{
		fprintf(stderr, "mp4_read_tfdt: version is not 1 and size is not 20 trackID: %u filehandler: %s \n", m->currtrakid, m->tracks[m->currtrakid].handler);
		return -1;
	}
//	cout<<"Decode time is "<<m->decodetime<<endl;
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


/*
 * The following flags are defined in the tf_flags:
•	0x01	base-data-offset-present:   indicates the presence of the base-data-offset field.  This provides an explicit anchor for the data offsets in each track run (see below). If not provided, the base-data-offset for the first track in the moof movie fragment is the position of the first byte of the enclosing  'moof' bMovie Fragment Box, and for second and subsequent track fragments, the default is the end of the data defined by the preceding fragment.  Fragments 'inheriting' their offset in this way should all use the same data-reference (i.e., the data for these tracks should be in the same file).
•	0x02	sample-description-m->index-present:   indicates the presence of this field, which over-rides, in this fragment, the default set up in the trexTrack Extends Box.
•	0x08	default-sample-duration-present
•	0x10	default-sample-size-present
•	0x20	default-sample-flags-present
•	0x100	duration-is-empty:   this indicates that the duration provided in either default-sample-duration, or by the default-duration in the trexTrack Extends Box, is empty, i.e. that there are no samples for this time interval. It is an error to make a movie presentation that has both edit lists in the moovMovie Box, and empty-duration fragments.
6.2.30.16.2.29.1	Syntax
aligned(8) class TrackFragmentHeaderBox
			extends FullBox(‘tfhd’, 0, tf_flags){
	unsigned int(32)	track-ID;
	// all the following are optional fields
	signed int(64)	base-data-offset;
	unsigned int(32)	sample-description-m->index;
	unsigned int(32)	default-sample-duration;
	unsigned int(32)	default-sample-size;
	unsigned int(32)	default-sample-flags
	source:Motion JPEG 2000JPEG2000 Study of Final Committee Draft 1.0
 *
 */


int mp4_read_tfhd(unsigned char * stream, int len, struct mp4_i * m)
{
	int offset;
	int ret =len;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->header = new unsigned char [m->li.size2];
		m->tocopy= m->li.size2;
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
	uint32_t flags = atouint32 ((char *)m->header, endianness);
	m->currtrakid = atouint32 ((char *)m->header+4, endianness);
	/* In mpgdash, the chunks/subsegments have same track ID, so erase the old info before proceeding. */
	if((m->tracks[m->currtrakid].stable!=NULL))
	{
		m->defsampledur=0;
		m->defsamplesize = 0;
		delete [] m->tracks[m->currtrakid].stable;
		m->tracks[m->currtrakid].stable=NULL;
	}

	offset=8;
	if(flags & 0x01)
	{
		//		We do nothing about the base offset except move the pointer forward
		offset += 8;

	}
	if(flags & 0x02)
	{
		/*sample description m->index is present*/
	//	cout<<"sdi is "<<atouint32 ((char *)m->header+offset, endianness)<<endl;
		offset+=4;
	}
	if(flags & 0x08)
	{
		/*sample duration is present*/
		m->defsampledur = atouint32 ((char *)m->header+offset, endianness);
	//	cout<<"sample duration is "<<m->defsampledur<<endl;
		offset+=4;
	}
	if(flags & 0x10)
	{
		/*sample size is present*/
		m->defsamplesize =atouint32 ((char *)m->header+offset, endianness);
	//	cout<<"sample size is "<<m->defsamplesize<<endl;
		offset+=4;

	}
	if(flags & 0x20)
	{
		/*sample flags is present*/
	//	cout<<"sample flags are "<<atouint32 ((char *)m->header+offset, endianness)<<endl;
	}


//	m->tracks[m->currtrakid].duration=0;
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
/*
 * Within the track Track fragment Fragment boxBox, there are zero or more track Track run Run boxesBoxes.  If the duration-is-empty flag is set in the tf_flags , there are no track runs. A track run documents a contiguous set of samples for a track.
The following flags are defined:
•	0x01	data-offset-present.  If the data-offset is not present, then the data for this run starts immediately after the data of the previous run, or at the base-data-offset defined by the track fragment m->header if this is the first run in a track fragment,
•	0x04	first-sample-flags-present;   this over-rides the default flags for the first sample only.  This makes it possible to record a group of frames where the first is a key and the rest are difference frames, without supplying explicit flags for every sample.  If this flag and field are used, sample-flags should not be present.
((http://code.google.com/p/mp4parser/source/browse/trunk/isoparser/src/main/java/com/coremedia/iso/boxes/fragment/TrackRunBox.java?r=878&spec=svn989)))

            if ((getFlags() & 0x100) == 0x100) { //sampleDurationPresent
                entry.sampleDuration = IsoTypeReader.readUInt32(content);
            }
            if ((getFlags() & 0x200) == 0x200) { //sampleSizePresent
                entry.sampleSize = IsoTypeReader.readUInt32(content);
            }
            if ((getFlags() & 0x400) == 0x400) { //sampleFlagsPresent
                entry.sampleFlags = new SampleFlags(content);
            }
            if ((getFlags() & 0x800) == 0x800) { //sampleCompositionTimeOffsetPresent6.2.31.16.2.30.1	Syntax
((This information was incorrect in the document. Find the right one!!!! )))

aligned(8) class TrackRunBox
			extends FullBox(‘trun’, 0, tr_flags) {
	unsigned int(32)	sample-count;
	// the following are optional fields
	signed int(32)	data-offset;
	unsigned int(32)	first-sample-flags;
	// all fields in the following array are optional
	{
		unsigned int(32)	sample-duration;
		unsigned int(32)	sample-size;
		unsigned int(32)	sample-flags
		unsigned int(32)	sample-composition-time-offset;
	}[ sample-count ]
 *
 */


int mp4_read_trun(unsigned char * stream, int len, struct mp4_i * m)
{
	uint32_t entries=0;
	uint32_t flags=0;
	int ret =len;
	//cout<<"length is"<<len<<endl;
	int offset;
	/*Check encoding to know the number of bytes for the datasize*/
	if(m->tocopy<0 || m->header==NULL)
	{
		m->header = new unsigned char [m->li.size2];
		m->tocopy=m->li.size2;
		m->index=0;
	}

	/*if m->header is incomplete*/
	if (len<m->tocopy)
	{
		if(m->table==NULL)
			memcpy(m->header+m->index, stream, len);
		m->index+=len;
		m->tocopy-=len;
		m->li.size2-=ret;
	//	printf("Not enough data, returning %d, m->tocopy %d \n", ret, m->tocopy);fflush(stdout);
		return ret;
	}
	memcpy(m->header+m->index, stream, m->tocopy);
	flags = atouint32 ((char *)m->header, endianness);
	entries= atouint32 ((char *)m->header+4, endianness);
	offset=8;
	if(flags & 0x01)
		offset += 4;
	if(flags & 0x04)
		offset+=4;



	if((m->tracks[m->currtrakid].stable!=NULL))
	{
		fprintf(stderr, "mp4_read_trun: stable is not empty. trackID: %d filehandler: %s \n", m->currtrakid, m->tracks[m->currtrakid].handler);
		return -1;
	}
	else
	{
		m->tracks[m->currtrakid].entries = entries;
		m->tracks[m->currtrakid].stable=new tts [entries];
		memzero(m->tracks[m->currtrakid].stable, sizeof(tts)*entries);
	}
	for(unsigned int i=0; i<entries; i++)
	{
		if(flags & 0x100)
		{
			m->tracks[m->currtrakid].stable[i].sdur = atouint32 ((char *)m->header+offset, endianness);
			offset+=4;
		}
		else
		{
			m->tracks[m->currtrakid].stable[i].sdur = m->defsampledur;
		}
	//	cout<<"Duration "<<m->tracks[m->currtrakid].stable[i].sdur;
		if(flags & 0x200)
		{
			m->tracks[m->currtrakid].stable[i].size = atouint32 ((char *)m->header+offset, endianness);
			offset+=4;
		//	cout<<"\t ActSize "<<m->tracks[m->currtrakid].stable[i].size<<endl;
		}
		else
		{
			m->tracks[m->currtrakid].stable[i].size = m->defsamplesize;
		//	cout<<"\t DefSize "<<m->tracks[m->currtrakid].stable[i].size<<endl;
		}

		if(flags & 0x400)
		{
		//	cout<<" sample flags present "<<atouint32 ((char *)m->header+offset, endianness)<<endl;
			offset+=4;
		}

		if(flags & 0x800)
		{
			//cout<<" sample composition flags present "<<atouint32 ((char *)m->header+offset, endianness)<<endl;
			m->tracks[m->currtrakid].stable[i].key=atouint32 ((char *)m->header+offset, endianness);
			offset+=4;
		}


	}

	m->tocopy = -1;
	m->index = 0;
	delete [] m->header;
	m->table = NULL;
//	cout<<"total size "<< m->li.size2<<"bytes"<<endl;

	ret = m->li.size2;
#ifdef DEBUG
	//printf("DEBUG: Datasize = %d\n", size);fflush(stdout);
#endif
	m->li.size2=0;
	checkmp4level(m);
	m->mp4state=MREADSIZE;
	return ret;


}


int mp4_read_mdat_a(unsigned char * stream, int len, struct mp4_i * m)
{
	unsigned int lastdur=m->decodetime;
	int ret=0;
	uint64_t baseoffset = m->rxbytesmp4 - len - 1; // -1 to go to last byte that has already been read.
	uint64_t  lastsize=0;
//	cout<<"baseoffset is "<<baseoffset<<endl;
	if (m->samplemap.size()==0)
	{
	//	cout<<"base offset is "<<baseoffset<<endl;
		for( map<uint32_t, struct tinfo>::iterator ii=m->tracks.begin(); ii!=m->tracks.end(); ii++)
		{
			if((*ii).second.stable== NULL)
			{
				cout<<"Received mdat and the sample m->table is still empty for one of the m->tracks "<<(*ii).first<<endl;
				return -1;
			}
			unsigned int i;
			for( i=0; i< (*ii).second.entries; i++)
			{
				lastsize += (*ii).second.stable[i].size;

				(*ii).second.stable[i].stime = (double)(lastdur)*1000/(double)m->timescale;/*convert seconds to ms by *1000*/
				(*ii).second.stable[i].offset = baseoffset+lastsize;
				double j = (*ii).second.stable[i].stime;
			//	cout<<"adding value for "<<(*ii).first<<" "<<i<<endl;
				if(m->samplemap[j] < (*ii).second.stable[i].offset)
					m->samplemap[j] = (*ii).second.stable[i].offset;
				if(Mp4Model)
					cout<<"youtubeevent13\t"<<m->tracks[m->currtrakid].handler<<"\t"<<(*ii).first<<"\t"<<(*ii).second.stable[i].stime<<"\t"<<m->segment<<"\t"\
					<<(*ii).second.stable[i].size<<"\t"<< (*ii).second.stable[i].sdur<<"\t"<<(*ii).second.stable[i].offset<<"\t"<<(*ii).second.stable[i].key<<endl;
				lastdur += (*ii).second.stable[i].sdur;

			//	writelog(tmp, strlen(tmp));

			}
			delete [] (*ii).second.stable;
			(*ii).second.stable = NULL;
		}

	}

	   for( map<double, uint64_t>::iterator ii=m->samplemap.begin(); ii!=m->samplemap.end();)
	   {
		//   exit(0);
	//       cout << (*ii).first << ": " << (*ii).second << endl;
		//   cout<<m->rxbytesmp4<<" "<<(*ii).second<<endl;
	       if(m->rxbytesmp4>(*ii).second)
	       {
	    	   baseoffset=(*ii).second;
		       metric.TSlist[m->stream] = (*ii).first;
	    	   m->samplemap.erase(ii++);
	       }
	       else
	    	   break;
	   }
	   if(m->samplemap.size()!=0)
	   {
		   m->li.size0-=len;
		   checkmp4level(m);
		   ret =  len;
	   }
	   else
	   {
		   ret = len-(m->rxbytesmp4-baseoffset)+1;/*bytes that have been read as part of mdat*/
		   m->li.size0-=ret;

		//   cout<<m->rxbytesmp4<<" boffset "<<baseoffset<<" len "<<len<<" return "<<ret<<" lisize0 "<<m->li.size0<<endl;;
		   checkmp4level(m);
			m->mp4state=MREADSIZE;


	   }
	   //memzero(stream, ret);
	   return ret;

}
