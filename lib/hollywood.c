/*
 * Copyright (c) 2016 University of Glasgow
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <organization> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * TCP Hollywood - Intermediary Layer
 */

#include "hollywood.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>

#define DEFAULT_TIMED_SUBSTREAM_ID 2
#define DEFAULT_UNTIMED_SUBSTREAM_ID 3

#ifndef __APPLE__
#define TCP_OODELIVERY 27
#define TCP_PRELIABILITY 28
#endif 

static inline void
timespec_add (struct timespec *sum, const struct timespec *left,
	              const struct timespec *right)
{
  sum->tv_sec = left->tv_sec + right->tv_sec;
  sum->tv_nsec = left->tv_nsec + right->tv_nsec;

  if (sum->tv_nsec >= 1000000000)
    {
      ++sum->tv_sec;
      sum->tv_nsec -= 1000000000;
    }
}

/* Message queue functions */
int add_message(hlywd_sock *socket, uint8_t *data, size_t len);
size_t dequeue_message(hlywd_sock *socket, uint8_t *buf, size_t len, uint8_t *substream_id, int *read_all);

/* Segment parsing */
void parse_segment(hlywd_sock *socket, uint8_t *segment, size_t segment_len, tcp_seq sequence_num);

/* Utility functions */
//void print_data(uint8_t *data, size_t data_len);

/* Sparsebuffer functions */

sparsebuffer *new_sbuffer();
void print_sbuffer_entry(sparsebuffer_entry *sb_entry);
void remove_sb_entry(sparsebuffer *sb, sparsebuffer_entry *sb_entry);
void destroy_sb_entry(sparsebuffer_entry *sb_entry);
void print_sbuffer(sparsebuffer *sb);
sparsebuffer_entry *add_entry(sparsebuffer *sb, tcp_seq sequence_num, size_t length, uint8_t *data);

/* Creates a new Hollywood socket */
int hollywood_socket(int fd, hlywd_sock *socket, int oo, int pr) {
	int flag = 1;

	/* Disable Nagle's algorithm (TCP_ NODELAY = 1) */
	int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

	/* Enable out-of-order delivery, if available */
	#ifdef TCP_OODELIVERY
	//printf("setting TCP_OODELIVERY to %d..\n", oo);
	result = setsockopt(fd, IPPROTO_TCP, TCP_OODELIVERY, (char *) &oo, sizeof(int));
	#endif

	/* Enable partial reliability, if available */
	#ifdef TCP_PRELIABILITY
	//printf("setting TCP_PRELIABILITY to %d..\n", pr);
	result = setsockopt(fd, IPPROTO_TCP, TCP_PRELIABILITY, (char *) &pr, sizeof(int));
	#endif

	/* Initialise Hollywood socket metadata */
	socket->sock_fd = fd;
	socket->message_q_head = NULL;
	socket->message_q_tail = NULL;
	socket->message_count = 0;
	socket->oo = oo;
    socket->pr = pr;
    socket->nb_send_buf = NULL;
	socket->nb_send_buf_len = 0;
	socket->nb_send_buf_offset = 0;

	socket->current_sequence_num = 0;
    socket->old_segments.index = 0;
    int i;
    for (i = 0; i < SEQNUM_MEMORYQ_LEN; i++) 
    {
        socket->old_segments.seq_num[i] = 0; 
    }
 
	socket->sb = new_sbuffer();
	return result;
}


int recv_nb(int fd, uint8_t *buffer, int len, int flags, int timeout) {
    fd_set readset;
    int result, iof = -1;
    struct timeval tv;
    
    FD_ZERO(&readset);
    FD_SET(fd, &readset);
    if (timeout > 0)
    {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        result = select(fd+1, &readset, NULL, NULL, &tv);
    }
    else
    {
        result = select(fd+1, &readset, NULL, NULL, NULL);
    }
   // printf("Return: %d \n", result);
    if (result < 0)
    {
        printf("Select failed for some reason\n");
        return -1;
    }
    else if (result > 0 && FD_ISSET(fd, &readset)) {
        return recv(fd, buffer, len, flags);
    }
    return -2;
}

int send_msg_retry(hlywd_sock *socket) {
    size_t write_size = socket->nb_send_buf_len-socket->nb_send_buf_offset;
	int send_rv = send(socket->sock_fd, socket->nb_send_buf+socket->nb_send_buf_offset, write_size, 0);
	if (send_rv > 0) {
	}
    if (send_rv > 0 && send_rv < write_size) {
        socket->nb_send_buf_offset += send_rv;
        errno = EWOULDBLOCK;
        return -1;
    } else if (send_rv == write_size) {
        free(socket->nb_send_buf);
        socket->nb_send_buf = NULL;
        socket->nb_send_buf_len = 0;
        socket->nb_send_buf_offset = 0;
        return 1;
    }
    errno = EWOULDBLOCK;
    return -1;
}

#ifdef TCP_PRELIABILITY
/* Sends a time-lined message */
ssize_t send_message_time_sub(hlywd_sock *socket, const uint8_t *buf, size_t len, int flags, uint32_t sequence_num, uint32_t depends_on, int lifetime_ms, uint8_t substream_id) {
    if (socket->pr == 1) {
	    /* Add sub-stream ID (2) to start of unencoded data */
	    uint8_t substream_id = 2;
	    uint8_t *preencode_buf = (uint8_t *) malloc(len+1);
	    memcpy(preencode_buf, &substream_id, 1);
	    memcpy(preencode_buf+1, buf, len);
	    len++;
	    /* max overhead of COBS is 0.4%, plus the leading and trailing \0 bytes */
	    size_t max_encoded_len = ceil(2 + 1.04*len);
	    size_t metadata_length = 9+sizeof(struct timespec)+sizeof(size_t);
	    uint8_t encoded_message[max_encoded_len+metadata_length];
	    /* add the leading \0, encoded message, and trailing \0 */
	    encoded_message[metadata_length] = '\0';
	    size_t encoded_len = cobs_encode(preencode_buf, len, encoded_message+metadata_length+1);
	    encoded_message[metadata_length+encoded_len+1] = '\0';
	    /* add partial reliability metadata */
	    size_t send_length = encoded_len+2;
	    encoded_message[0] = substream_id;
	    memcpy(encoded_message+1, &send_length, sizeof(size_t));
	    memcpy(encoded_message+1+sizeof(size_t), &sequence_num, 4);
	    /* deadline calculation */
	    struct timespec deadline;
	    struct timespec lifetime;
	    lifetime.tv_sec = (lifetime_ms * 1000000) / 1000000000;
	    lifetime.tv_nsec = (lifetime_ms * 1000000) % 1000000000;
        struct timespec current_time;
        clock_gettime(CLOCK_REALTIME, &current_time);
        timespec_add(&deadline, &current_time, &lifetime);
        memcpy(encoded_message+1+sizeof(size_t)+4, &deadline, sizeof(struct timespec));
	    memcpy(encoded_message+1+sizeof(size_t)+4+sizeof(struct timespec), &depends_on, 4);
	    /* free pre-encoding buffer */
	    free(preencode_buf);
	    /* send, returning sent size to application */
	    size_t write_size = encoded_len+2+metadata_length;
        int send_rv = send(socket->sock_fd, encoded_message, write_size, flags);
        send_rv += metadata_length;
        if (send_rv < 0 && errno == EWOULDBLOCK) {
            socket->nb_send_buf = realloc(socket->nb_send_buf, socket->nb_send_buf_len+write_size);
            socket->nb_send_buf_len = write_size;
            memcpy(socket->nb_send_buf, encoded_message, write_size);
            errno = EWOULDBLOCK;
            return -1;
        } else if (send_rv < write_size) {
            socket->nb_send_buf = realloc(socket->nb_send_buf, socket->nb_send_buf_len+(write_size-send_rv));
            socket->nb_send_buf_len = write_size-send_rv;
            memcpy(socket->nb_send_buf, encoded_message+send_rv, write_size-send_rv);
            errno = EWOULDBLOCK;
            return -1;
        }
        return 1;
    } else {
        return send_message(socket, buf, len, flags);
    }
}
#else
ssize_t send_message_time_sub(hlywd_sock *socket, const uint8_t *buf, size_t len, int flags, uint32_t sequence_num, uint32_t depends_on, int lifetime_ms, uint8_t substream_id) {
	return send_message_sub(socket, buf, len, flags, substream_id);
}
#endif

ssize_t send_message_time(hlywd_sock *socket, const uint8_t *buf, size_t len, int flags, uint32_t sequence_num, uint32_t depends_on, int lifetime_ms) {
    return send_message_time_sub(socket, buf, len, flags, sequence_num, depends_on, lifetime_ms, DEFAULT_TIMED_SUBSTREAM_ID);
}

/* Sends a non-time-lined message */
ssize_t send_message_sub(hlywd_sock *socket, const uint8_t *buf, size_t len, int flags, uint8_t substream_id) {
    if (socket->pr == 1) {
        uint8_t *preencode_buf = (uint8_t *) malloc(len+1);
        memcpy(preencode_buf, &substream_id, 1);
        memcpy(preencode_buf+1, buf, len);
        len++;
        /* max overhead of COBS is 0.4%, plus the leading and trailing \0 bytes */
        size_t max_encoded_len = ceil(2 + 1.04*len);
        size_t metadata_length = 9+sizeof(struct timespec)+sizeof(size_t);
        uint8_t encoded_message[max_encoded_len+metadata_length];
        /* add the leading \0, encoded message, and trailing \0 */
        encoded_message[metadata_length] = '\0';
        size_t encoded_len = cobs_encode(preencode_buf, len, encoded_message+metadata_length+1);
        encoded_message[metadata_length+encoded_len+1] = '\0';
        size_t send_length = encoded_len+2;
        /* add metadata */
        encoded_message[0] = substream_id;
        memcpy(encoded_message+1, &send_length, sizeof(size_t));
        memset(encoded_message+1+sizeof(size_t), 0, metadata_length-1-sizeof(size_t));
        /* free pre-encoding buffer */
        free(preencode_buf);
        size_t write_size = encoded_len+2+metadata_length;
        int send_rv = send(socket->sock_fd, encoded_message, write_size, flags);
        send_rv += metadata_length;
        if (send_rv < 0 && errno == EWOULDBLOCK) {
            socket->nb_send_buf = realloc(socket->nb_send_buf, socket->nb_send_buf_len+write_size);
            socket->nb_send_buf_len = write_size;
            memcpy(socket->nb_send_buf, encoded_message, write_size);
            errno = EWOULDBLOCK;
            return -1;
        } else if (send_rv < write_size) {
            socket->nb_send_buf = realloc(socket->nb_send_buf, socket->nb_send_buf_len+(write_size-send_rv));
            socket->nb_send_buf_len = write_size-send_rv;
            memcpy(socket->nb_send_buf, encoded_message+send_rv, write_size-send_rv);
            errno = EWOULDBLOCK;
            return -1;
        }
        return 1;
    } else {
        uint8_t *preencode_buf = (uint8_t *) malloc(len+1);
        memcpy(preencode_buf, &substream_id, 1);
        memcpy(preencode_buf+1, buf, len);
        len++;
        int i;
        /* max overhead of COBS is 0.4%, plus the leading and trailing \0 bytes */
        size_t max_encoded_len = ceil(2 + 1.04*len);
        uint8_t encoded_message[max_encoded_len];
        /* add the leading \0, encoded message, and trailing \0 */
        encoded_message[0] = '\0';
        size_t encoded_len = cobs_encode(preencode_buf, len, encoded_message+1);
        encoded_message[encoded_len+1] = '\0';
        /* free pre-encoding buffer */
        free(preencode_buf);
        size_t write_size = encoded_len+2;
        int send_rv = send(socket->sock_fd, encoded_message, write_size, flags);
        if (send_rv < 0 && errno == EWOULDBLOCK) {
            socket->nb_send_buf = realloc(socket->nb_send_buf, socket->nb_send_buf_len+write_size);
            socket->nb_send_buf_len = write_size;
            memcpy(socket->nb_send_buf, encoded_message, write_size);
            errno = EWOULDBLOCK;
            return -1;
        } else if (send_rv < write_size) {
            socket->nb_send_buf = realloc(socket->nb_send_buf, socket->nb_send_buf_len+(write_size-send_rv));
            socket->nb_send_buf_len = write_size-send_rv;
            memcpy(socket->nb_send_buf, encoded_message+send_rv, write_size-send_rv);
            errno = EWOULDBLOCK;
            return -1;
        }
        return 1;
    }
}

/* Sends a message */
ssize_t send_message(hlywd_sock *socket, const uint8_t *buf, size_t len, int flags) {
	return send_message_sub(socket, buf, len, flags, DEFAULT_UNTIMED_SUBSTREAM_ID);
}

/* Calculates maximum encoded message size */
size_t encoded_len(size_t len) {
	return ceil(2 + 1.04*len) + 2;
}

int is_duplicate(hlywd_sock *socket, tcp_seq sequence_num)
{
	int i;
    for (i = 0; i < SEQNUM_MEMORYQ_LEN; i++) 
    {
        if (sequence_num == socket->old_segments.seq_num[i])
            return 1; 
    }
    socket->old_segments.seq_num[socket->old_segments.index] = sequence_num; 
    socket->old_segments.index++; 
    if(socket->old_segments.index >= SEQNUM_MEMORYQ_LEN)
        socket->old_segments.index = 0; 
    return 0;

}

/* Receives a message */
ssize_t recv_message(hlywd_sock *socket, uint8_t *buf, size_t len, int flags, uint8_t *substream_id, int timeout_s, int *read_all) {
	while (socket->message_count == 0) {
		uint8_t segment[1500+sizeof(tcp_seq)];
		tcp_seq sequence_num = 0;
		ssize_t segment_len;
        //printf("(Hollywood[%d:%d]... ",socket->sock_fd, flags);
        //fflush(stdout);
        if(timeout_s>0) {
    		segment_len = recv_nb(socket->sock_fd, segment, 1500+sizeof(tcp_seq), flags, timeout_s);
    	}
        else {   
		    segment_len = recv(socket->sock_fd, segment, 1500+sizeof(tcp_seq), flags);
		}
		if (segment_len <= 0) {
			return segment_len;
		}
		if (socket->oo) {
			if (segment_len >= sizeof(tcp_seq)) {
				memcpy(&sequence_num, segment+(segment_len-4), 4);
				segment_len -= 4;
			//	if (is_duplicate(socket, sequence_num))
			//	    continue; 
			}
		} else {
		    //fprintf(stderr, "[hlywd_lib] receiving %d\n", segment_len);
		    //fflush(stderr);
			sequence_num = socket->current_sequence_num;
			socket->current_sequence_num += segment_len;
		}
	    //fprintf(stderr, "[hlywd_lib] sbuf before\n");
	    print_sbuffer(socket->sb);
	    //fprintf(stderr, "[hlywd_lib] ---\n");
		parse_segment(socket, segment, segment_len, sequence_num);
	    //fprintf(stderr, "[hlywd_lib] sbuf after\n");
	    print_sbuffer(socket->sb);
	    //fprintf(stderr, "[hlywd_lib] ---\n");
        //fflush(stderr);
	}
	return dequeue_message(socket, (uint8_t *)buf, len, substream_id, read_all);
}

/* Message queue functions */
	
/* Removes the message at the head of the message queue */
size_t dequeue_message(hlywd_sock *socket, uint8_t *buf, size_t len, uint8_t *substream_id, int *read_all) {
	if (socket->message_count > 0) {
	    size_t bytes_to_read = MIN(socket->message_q_head->len-socket->message_q_head->bytes_read, len);
	    //printf("dequeue_message (msg_len %d, bytes_read %d, read_len %d, bytes_to_read %d)\n", socket->message_q_head->len, socket->message_q_head->bytes_read, len, bytes_to_read);
	    memcpy(buf, socket->message_q_head->data+socket->message_q_head->bytes_read, bytes_to_read);
	    memcpy(substream_id, &socket->message_q_head->substream_id, 1);
	    socket->message_q_head->bytes_read += bytes_to_read;
	    if (socket->message_q_head->bytes_read == socket->message_q_head->len) {
		    message *dequeued_msg = socket->message_q_head;
		    socket->message_q_head = dequeued_msg->next;
		    free(dequeued_msg->data-1);
			free(dequeued_msg);
			socket->message_count--;
			*read_all = 1;
                    //printf("read everything -- dequeueing!!\n");
	    }
	    //printf("Hollywood: bytes to read: %zu\n", bytes_to_read);
	    if (bytes_to_read == 0) {
		return -1;
	    } else {
	        return bytes_to_read;
            }
	} else {
		return -1;
	}
}

/* Adds a message to the end of the message queue */
int add_message(hlywd_sock *socket, uint8_t *data, size_t len) {
    if (len == 0) {
        return 0;
    }
    //fflush(stderr);
	message *new_message = (message *) malloc(sizeof(message));
	memcpy(&new_message->substream_id, data, 1);
	new_message->data = data+1;
	new_message->len = len-1;
	new_message->bytes_read = 0;
	new_message->next = NULL;
	if (socket->message_q_head == NULL) {
		socket->message_q_head = new_message;
		socket->message_q_tail = new_message;
	} else {
		socket->message_q_tail->next = new_message;
		socket->message_q_tail = new_message;
	}
	return socket->message_count++;
}

/* Segment parsing */

/* Parse an incoming segment */
void parse_segment(hlywd_sock *socket, uint8_t *segment, size_t segment_len, tcp_seq sequence_num) {
    //fprintf(stderr, "segment (seq %zu, len %zu)\n", sequence_num, segment_len);
    //fflush(stderr);
	int message_region_start = 0;
	int message_region_end = segment_len-1;
	/* check for head */
	int head_end = 0;
	int head_len = 0;
	sparsebuffer_entry *sb_head_entry = NULL;
	sparsebuffer_entry *sb_tail_entry = NULL;
	/* check for single NULL byte segments -- a bit hacky, look at revising logic below */
    if (segment_len == 1 && segment[0] == '\0') {
        sb_head_entry = add_entry(socket->sb, sequence_num, segment_len, segment);
        if (sb_head_entry->len != 1) {
            remove_sb_entry(socket->sb, sb_head_entry);
            parse_segment(socket, sb_head_entry->data, sb_head_entry->len, sb_head_entry->sequence_num);
            destroy_sb_entry(sb_head_entry);
        }
        return;
    }
	while (head_end < segment_len && segment[head_end] != '\0') {
		head_end++;
	}
	if (head_end > 0 || (head_end < segment_len && segment[head_end+1] == '\0') || (head_end == segment_len)) {
		if (head_end == segment_len) {
			head_len = segment_len;
		} else {
			head_len = head_end+1;
		}
		sb_head_entry = add_entry(socket->sb, sequence_num, head_len, segment);
		message_region_start = head_end + 1;
		if (sb_head_entry->len != head_len) {
			remove_sb_entry(socket->sb, sb_head_entry);
			parse_segment(socket, sb_head_entry->data, sb_head_entry->len, sb_head_entry->sequence_num);
			destroy_sb_entry(sb_head_entry);
		}
	}
	/* check for tail */
	int tail_start = segment_len-1;
	while (tail_start > head_end && segment[tail_start] != '\0') {
		tail_start--;
	}
	if (tail_start < segment_len-1 || (tail_start-1 >= 0 && segment[tail_start-1] == '\0')) {
		message_region_end = tail_start-1;
		sb_tail_entry = add_entry(socket->sb, sequence_num+tail_start, segment_len-tail_start, segment+tail_start);
		if (sb_tail_entry->len != segment_len-tail_start && sb_tail_entry->sequence_num != sequence_num) {
				remove_sb_entry(socket->sb, sb_tail_entry);
				parse_segment(socket, sb_tail_entry->data, sb_tail_entry->len, sb_tail_entry->sequence_num);
				destroy_sb_entry(sb_tail_entry);
		}
	}
	/* check for full messages */
	int current_byte = message_region_start;
	while (current_byte < message_region_end) {
		int msg_start = current_byte;
		current_byte++;
		while (segment[current_byte] != '\0') {
			current_byte++;
		}
		int msg_end = current_byte;
		/* decode received message */
		uint8_t substream_id;
		uint8_t *decoded_msg = (uint8_t *) malloc(msg_end-msg_start-1);
		//printf("decoded_msg_len is %d\n", decoded_msg);
		size_t decoded_msg_len = cobs_decode(segment+msg_start+1, msg_end-msg_start-1, decoded_msg);
		memcpy(&substream_id, decoded_msg, 1);
		add_message(socket, decoded_msg, decoded_msg_len);
		current_byte++;
	}
	
}

/* Utility functions */

//void print_data(uint8_t *data, size_t len) {
//	printf("[");
//	int i;
//	for (i = 0; i < len; i++) {
//		if (data[i] == '\0') {
//			printf("0");
//		} else {
//			printf(".");
//		}
//	}
//	printf("]\n");
//}

/* Sparsebuffer functions */

sparsebuffer *new_sbuffer() {
	sparsebuffer *sb = (sparsebuffer *) malloc(sizeof(sparsebuffer));
	sb->head = NULL;
	sb->tail = NULL;
	return sb;
}

void print_sbuffer_entry(sparsebuffer_entry *sb_entry) {
	if (sb_entry == NULL) {
		fprintf(stderr, "NULL\n");
	} else {
		fprintf(stderr, "[%u, len: %zu] (", sb_entry->sequence_num, sb_entry->len);
		int i;
		for (i = 0; i < sb_entry->len; i++) {
			if (sb_entry->data[i] == '\0') {
				fprintf(stderr, "0");
			} else {
				fprintf(stderr, ".");
			}
		}
		fprintf(stderr, ")\n");
	}
    fflush(stderr);
}

void remove_sb_entry(sparsebuffer *sb, sparsebuffer_entry *sb_entry) {
	if (sb_entry->prev == NULL) {
		sb->head = sb_entry->next;
	} else {
		sb_entry->prev->next = sb_entry->next;
	}
	if (sb_entry->next == NULL) {
		sb->tail = sb_entry->prev;
	} else {
		sb_entry->next->prev = sb_entry->prev;
	}
}

void destroy_sb_entry(sparsebuffer_entry *sb_entry) {
	free(sb_entry->data);
	free(sb_entry);
}

void print_sbuffer(sparsebuffer *sb) {
	sparsebuffer_entry *sb_entry = sb->head;
	while (sb_entry != NULL) {
		//print_sbuffer_entry(sb_entry);
		sb_entry = sb_entry->next;
	}
}

sparsebuffer_entry *add_entry(sparsebuffer *sb, tcp_seq sequence_num, size_t length, uint8_t *data) {
    /* find starts_in */
    sparsebuffer_entry *starts_in = sb->head;
    while (starts_in != NULL && sequence_num > starts_in->sequence_num+starts_in->len) {
        starts_in = starts_in->next;
    }
    /* find ends_in */
    sparsebuffer_entry *ends_in = starts_in;
    while (ends_in != NULL && sequence_num+length >= ends_in->sequence_num+ends_in->len) {
        ends_in = ends_in ->next;
    }
    /* construct new entry */
    sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
    //printf(">>>\n");
    //print_sbuffer_entry(starts_in);
    /* populate sequence number */
    if (starts_in != NULL && starts_in->sequence_num < sequence_num) {
        new_sbe->sequence_num = starts_in->sequence_num;
    } else {
        new_sbe->sequence_num = sequence_num;
    }
    /* populate data length */
    if (ends_in != NULL && sequence_num+length >= ends_in->sequence_num) {
        new_sbe->len = ends_in->sequence_num+ends_in->len - new_sbe->sequence_num;
    } else {
        new_sbe->len = sequence_num+length - new_sbe->sequence_num;
    }
    /* malloc data space */
    new_sbe->data = (uint8_t *) malloc(new_sbe->len);
    /* copy data from starts_in, if it exists */
    int current_pos = 0;
    if (new_sbe->sequence_num < sequence_num) {
        /* data to copy from starts_in */
        current_pos = sequence_num-new_sbe->sequence_num;
        memcpy(new_sbe->data, starts_in->data, current_pos);
    }
    /* copy from data */
    memcpy(new_sbe->data+current_pos, data, length);
    current_pos += length;
    if (new_sbe->sequence_num+new_sbe->len > sequence_num+length) {
        /* data to copy from ends_in */
        memcpy(new_sbe->data+current_pos, ends_in->data+(ends_in->len-(new_sbe->len-current_pos)), new_sbe->len-current_pos);
    }
    /* wire in new_sbe */
    /* starts_in */
    if (starts_in != NULL) {
        new_sbe->prev = starts_in->prev;
        if (starts_in->prev != NULL) {
            starts_in->prev->next = new_sbe;
        } else {
            sb->head = new_sbe;
        }
    } else {
        if (sb->head == NULL) {
            new_sbe->prev = NULL;
            sb->head = new_sbe;
        } else {
            new_sbe->prev = sb->tail;
            sb->tail->next = new_sbe;
            sb->tail = new_sbe;
        }
    }
    /* ends_in */
    if (ends_in != NULL) {
        if (new_sbe->sequence_num+new_sbe->len < ends_in->sequence_num) {
            new_sbe->next = ends_in;
            ends_in->prev = new_sbe;
        } else {
            new_sbe->next = ends_in->next;
            if (ends_in->next != NULL) {
                ends_in->next->prev = new_sbe;
            } else {
                sb->tail = new_sbe;
                new_sbe->next = NULL;
            }
        }
    } else {
        sb->tail = new_sbe;
        new_sbe->next = NULL;
    }
    /* clean up */  
    sparsebuffer_entry* del = starts_in;
    while (del != NULL && del != ends_in) {
        sparsebuffer_entry* del_next = del->next;
        destroy_sb_entry(del);
        del = del_next;
    }
    if (ends_in != NULL && new_sbe->sequence_num+new_sbe->len >= ends_in->sequence_num) {
        destroy_sb_entry(ends_in);
    }
    return new_sbe;
}

void destroy_socket(hlywd_sock *sock) {
    free(sock->sb);
}
