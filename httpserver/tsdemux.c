/*
 * Based on tsdemux (Copyright (C) 2009 Anton Burdinuk; MIT licence)
 */

#include "tsdemux.h"

stream *streams = NULL;

int validate_type(uint8_t type) {
    return strchr("\x01\x02\x80\x10\x1b\x24\xea\x1f\x20\x21\x03\x04\x11\x1c\x0f\x81\x06\x83\x84\x87\x82\x86\x8a\x85",type) ? 1 : 0;
}

uint8_t to_byte(const char* p) {
    return *((unsigned char*)p);
}

uint16_t to_int(const char* p) {
    uint16_t n=((unsigned char*)p)[0];
    n<<=8;
    n+=((unsigned char*)p)[1];
    return n;
}

uint64_t decode_pts(const char* ptr) {
    const unsigned char *p = (const unsigned char*) ptr;

    uint64_t pts=((p[0]&0xe)<<29);
    pts|=((p[1]&0xff)<<22);
    pts|=((p[2]&0xfe)<<14);
    pts|=((p[3]&0xff)<<7);
    pts|=((p[4]&0xfe)>>1);

    return pts;
}

stream *get_stream(uint16_t pid) {
    stream *cur_stream = streams;
    stream *prev_stream = streams;
    while (cur_stream != NULL && cur_stream->pid != pid) {
        prev_stream = cur_stream;
        cur_stream = cur_stream->next;
    }
    if (cur_stream == NULL) {
        stream *new_stream = (stream *) malloc(sizeof(stream));
        new_stream->channel = 0xffff;
        new_stream->id = 0;
        new_stream->stream_id = 0;
        new_stream->type = 0xff;
        new_stream->dts = 0;
        new_stream->first_dts = 0;
        new_stream->first_pts = 0;
        new_stream->frame_length = 0;
        new_stream->frame_num = 0;
        new_stream->pid = pid;
        new_stream->next = NULL;
        if (prev_stream == NULL) {
            streams = new_stream;
        } else {
            prev_stream->next = new_stream;
        }
        return new_stream;
    } else {
        return cur_stream;
    }
}   

vid_frame *get_frames(struct parse_attr *p) {        
    /* open TS file */
    FILE *ts = fopen(p->src_filename, "r");

    uint64_t packet_num = 1;
    char pkt_buf[188];
    
    size_t bytes_read_total = 0;
    
    vid_frame *frames = (vid_frame *) malloc(sizeof(vid_frame));;
    vid_frame *last_frame = frames;
    last_frame->starts_at = -1;
    last_frame->len = 0;
    last_frame->key_frame = 1;
    last_frame->next = NULL;
    
    for(packet_num = 1;;packet_num++) {
        size_t bytes_read = fread(pkt_buf, 1, 188, ts);
        bytes_read_total += bytes_read;

        if (bytes_read != 188) {
            break;
        }

        const char *ptr = pkt_buf;
        const char *end_ptr = ptr + 188;
        
        if (ptr[0] != 0x47) {
            /* TS sync byte */
            break;
        }
                
        uint16_t pid = to_int(ptr+1);
        uint8_t flags = to_byte(ptr+3);
    
        int transport_error = pid & 0x8000;
        int payload_unit_start_indicator = pid & 0x4000;
        int adaptation_field_exist = flags & 0x20;
        int payload_data_exist = flags & 0x10;
        pid &= 0x1fff;

        if (transport_error)
            return NULL;

        if (pid==0x1fff || !payload_data_exist)
            return NULL;

        ptr+=4;

        // skip adaptation field
        if (adaptation_field_exist) {
            ptr += to_byte(ptr)+1;
            if (ptr>=end_ptr)
                return NULL;
        }
                        
        stream *s = get_stream(pid);
        
        if(!pid || (s->channel!=0xffff && s->type==0xff)) {
            // PSI
            if (payload_unit_start_indicator) {
                // begin of PSI table
                ptr++;
                
                if (ptr>=end_ptr) {
                    return NULL;
                }

                if (*ptr!=0x00 && *ptr!=0x02) {
                    return NULL;
                }
                
                if (end_ptr-ptr<3) {
                    return NULL;
                }
                
                uint16_t l = to_int(ptr+1);

                if (l & 0x3000 != 0x3000) {
                    return NULL;
                }
                
                l &= 0x0fff;

                ptr+=3;

                int len = end_ptr-ptr;

                if (l > len) {
                    if (l > 512) {
                        return -7;
                    }

                    s->psi.len = 0;
                    s->psi.offset = 0;

                    memcpy(s->psi.buf,ptr,len);
                    s->psi.offset += len;
                    s->psi.len = l;

                    return 0;
                } else
                    end_ptr = ptr+l;
            } else {
                // next part of PSI
                if (!s->psi.offset) {
                    return -8;
                }

                int len = end_ptr-ptr;

                if (len>512-s->psi.offset) {
                    return -9;
                }

                memcpy(s->psi.buf+s->psi.offset,ptr,len);
                s->psi.offset+=len;

                if (s->psi.offset<s->psi.len) {
                    return 0;
                } else {
                    ptr = s->psi.buf;
                    end_ptr = ptr+s->psi.len;
                }
            }

            if (!pid) {
                // PAT
                ptr+=5;

                if (ptr>=end_ptr)
                    return -10;

                int len=end_ptr-ptr-4;

                if(len<0 || len%4) {
                    return -11;
                }

                int n=len/4;

                for(int i=0;i<n;i++,ptr+=4) {
                    uint16_t channel=to_int(ptr);
                    uint16_t pid=to_int(ptr+2);

                    if (pid&0xe000!=0xe000) {
                        return -12;
                    }

                    pid &= 0x1fff;
                    
                    stream *ss = get_stream(pid);
                    ss->channel = channel;
                    ss->type = 0xff;
                }
            } else {
                // PMT
                ptr+=7;

                if (ptr>=end_ptr) {
                    return NULL;
                }
                
                uint16_t info_len = to_int(ptr) & 0x0fff;

                ptr+=info_len+2;
                end_ptr-=4;

                if(ptr>=end_ptr) {
                    return NULL;
                }

                while (ptr<end_ptr) {
                    if (end_ptr-ptr<5) {
                        return NULL;
                    }
                    
                    uint8_t type=to_byte(ptr);
                    uint16_t pid=to_int(ptr+1);

                    if (pid&0xe000!=0xe000) {
                        return NULL;
                    }
                    
                    pid &= 0x1fff;

                    info_len = to_int(ptr+3)&0x0fff;

                    ptr+=5+info_len;

                    // ignore unknown streams
                    if(validate_type(type)) {
                        stream *ss = get_stream(pid);

                        if (ss->channel!=s->channel || ss->type!=type) {
                            ss->channel = s->channel;
                            ss->type = type;
                            ss->id = ++s->id;
                        }
                    }
                }

                if (ptr!=end_ptr) {
                    return NULL;
                }
            }
        } else {
            if (s->type!=0xff) {
                // PES
                if (payload_unit_start_indicator) {
                    s->psi.len = 9;
                    s->psi.offset = 0;
                }
                
                while (s->psi.offset<s->psi.len) {
                    int len=end_ptr-ptr;
                    
                    if(len<=0) {
                        return NULL;
                    }

                    int n=s->psi.len-s->psi.offset;

                    if(len>n)
                        len=n;

                    memcpy(s->psi.buf+s->psi.offset,ptr,len);
                    s->psi.offset+=len;

                    ptr+=len;

                    if(s->psi.len==9)
                        s->psi.len+=to_byte(s->psi.buf+8);
                }

                if (s->psi.len) {
                    if (memcmp(s->psi.buf,"\x00\x00\x01",3)) {
                        return NULL;
                    }
                        
                    s->stream_id=to_byte(s->psi.buf+3);

                    uint8_t flags=to_byte(s->psi.buf+7);

                    s->frame_num++;
                    if (last_frame->starts_at == -1 && last_frame->len > 0) {
                        last_frame->starts_at = 0;
                    } else {
                        vid_frame *next_frame = (vid_frame *) malloc(sizeof(vid_frame));
                        next_frame->starts_at = last_frame->starts_at + last_frame->len;
                        next_frame->len = 0;
                        next_frame->key_frame = 0;
                        next_frame->next = NULL;
                        last_frame->next = next_frame;
                        last_frame = next_frame;
                    }

                    switch(flags&0xc0) {
                        case 0x80:          // PTS only
                            {
                                uint64_t pts = decode_pts(s->psi.buf+9);

                                last_frame->timestamp = DIV_ROUND_CLOSEST(pts, 90);
                                
                                if(s->dts>0 && pts>s->dts)
                                    s->frame_length=pts-s->dts;
                                s->dts=pts;
                        
                                if(pts>s->last_pts)
                                    s->last_pts=pts;

                                if(!s->first_pts)
                                    s->first_pts=pts;
                            }
                            break;
                        case 0xc0:          // PTS,DTS
                            {
                                uint64_t pts=decode_pts(s->psi.buf+9);
                                uint64_t dts=decode_pts(s->psi.buf+14);

                                if (s->dts>0 && dts>s->dts)
                                    s->frame_length = dts-s->dts;
                                s->dts=dts;
                        
                                if(pts>s->last_pts)
                                    s->last_pts=pts;

                                if(!s->first_dts)
                                    s->first_dts=dts;
                            }
                            break;
                    }

                    s->psi.len = 0;
                    s->psi.offset = 0;
                }
        }
    }
    
    last_frame->len += bytes_read;
    
    }
    
    stream *cur_stream = p->streams;
    while (cur_stream != NULL) {
        stream *next_stream = cur_stream->next;
        free(cur_stream);
        cur_stream = next_stream;
    }
    
    /* close TS file */
    fclose(ts);
    
    return frames;
}