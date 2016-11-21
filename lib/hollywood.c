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

#ifndef __APPLE__
    #define TCP_OODELIVERY 27
#endif

/* Message queue functions */
int add_message(hlywd_sock *socket, uint8_t *data, size_t len);
size_t dequeue_message(hlywd_sock *socket, uint8_t *buf, uint8_t *substream_id);

/* Segment parsing */
void parse_segment(hlywd_sock *socket, uint8_t *segment, size_t segment_len, tcp_seq sequence_num);

/* Utility functions */
void print_data(uint8_t *data, size_t data_len);

/* Sparsebuffer functions */

sparsebuffer *new_sbuffer();
void print_sbuffer_entry(sparsebuffer_entry *sb_entry);
void remove_sb_entry(sparsebuffer *sb, sparsebuffer_entry *sb_entry);
void destroy_sb_entry(sparsebuffer_entry *sb_entry);
void print_sbuffer(sparsebuffer *sb);
sparsebuffer_entry *add_entry(sparsebuffer *sb, tcp_seq sequence_num, size_t length, uint8_t *data);

/* Creates a new Hollywood socket */
int hollywood_socket(int fd, hlywd_sock *socket, int oo) {
	int flag = 1;

	/* Disable Nagle's algorithm (TCP_NODELAY = 1) */
	int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

	/* Enable out-of-order delivery, if available */
	#ifdef TCP_OODELIVERY
	result = setsockopt(fd, IPPROTO_TCP, TCP_OODELIVERY, (char *) &oo, sizeof(int));
	#endif

	/* Enable partial reliability, if available */
	#ifdef TCP_PRELIABILITY
	result = setsockopt(fd, IPPROTO_TCP, TCP_PRELIABILITY, (char *) &flag, sizeof(int));
	#endif

	/* Initialise Hollywood socket metadata */
	socket->sock_fd = fd;
	socket->message_q_head = NULL;
	socket->message_q_tail = NULL;
	socket->message_count = 0;

	socket->current_sequence_num = 0;

	socket->sb = new_sbuffer();
	return result;
}

/* Set the play-out delay to pd_ms, a value in ms */
void set_playout_delay(hlywd_sock *socket, int pd_ms) {
	int pd_ns = pd_ms * 1000000;
	socket->playout_delay.tv_sec = pd_ns / 1000000000;
	socket->playout_delay.tv_nsec = pd_ns % 1000000000;
}

#ifdef TCP_PRELIABILITY
/* Sends a non-time-lined message */
ssize_t send_message_sub(hlywd_sock *socket, const void *buf, size_t len, int flags, uint8_t substream_id) {
	if (substream_id < 3) {
		return 0;
	}
	/* Add sub-stream ID to start of unencoded data */
	uint8_t *preencode_buf = (uint8_t *) malloc(len+1);
	memcpy(preencode_buf, &substream_id, 1);
	memcpy(preencode_buf+1, buf, len);
	len++;
	/* max overhead of COBS is 0.4%, plus the leading and trailing \0 bytes */
	size_t max_encoded_len = ceil(2 + 1.04*len);
	uint8_t encoded_message[max_encoded_len+1];
	/* add the leading \0, encoded message, and trailing \0 */
	encoded_message[1] = '\0';
	size_t encoded_len = cobs_encode(preencode_buf, len, encoded_message+2);
	encoded_message[encoded_len+2] = '\0';
	/* add unencoded sub-stream ID, for use by kernel */
	encoded_message[0] = substream_id;
	/* free pre-encoding buffer */
	free(preencode_buf);
	/* send, returning sent size to application */
	return send(socket->sock_fd, encoded_message, encoded_len+3, flags);
}

/* Sends a time-lined message */
ssize_t send_message_time(hlywd_sock *socket, const void *buf, size_t len, int flags, uint16_t sequence_num, uint16_t depends_on, int lifetime_ms) {
	/* Add sub-stream ID (2) to start of unencoded data */
	uint8_t substream_id = 2;
	uint8_t *preencode_buf = (uint8_t *) malloc(len+1);
	memcpy(preencode_buf, &substream_id, 1);
	memcpy(preencode_buf+1, buf, len);
	len++;
	/* max overhead of COBS is 0.4%, plus the leading and trailing \0 bytes */
	size_t max_encoded_len = ceil(2 + 1.04*len);
	uint8_t encoded_message[max_encoded_len+5+2*sizeof(time_t)+2*sizeof(suseconds_t)];
	/* add the leading \0, encoded message, and trailing \0 */
	encoded_message[5+5+2*sizeof(time_t)+2*sizeof(suseconds_t)] = '\0';
	size_t encoded_len = cobs_encode(preencode_buf, len, encoded_message+5+2*sizeof(time_t)+2*sizeof(suseconds_t)+1);
	encoded_message[encoded_len+5+2*sizeof(time_t)+2*sizeof(suseconds_t)+1] = '\0';
	/* add partial reliability metadata */
	encoded_message[0] = 2;
	memcpy(encoded_message+1, &sequence_num, 2);
	memcpy(encoded_message+3, &depends_on, 2);
	struct timespec lifetime;
	lifetime.tv_sec = (lifetime_ms) / 1000;
	lifetime.tv_usec = (lifetime_ms * 1000000) % 1000000000;
	memcpy(encoded_message+5, &lifetime, sizeof(struct timespec));
	memcpy(encoded_message+5+sizeof(struct timespec), &socket->playout_delay, sizeof(struct timespec));
	/* free pre-encoding buffer */
	free(preencode_buf);
	/* send, returning sent size to application */
	return send(socket->sock_fd, encoded_message, encoded_len+5+2*sizeof(struct timespec)+2, flags);
}
#else
/* Sends a non-time-lined message */
ssize_t send_message_sub(hlywd_sock *socket, const void *buf, size_t len, int flags, uint8_t substream_id) {
	/* Add sub-stream ID to start of unencoded data */
	uint8_t *preencode_buf = (uint8_t *) malloc(len+1);
	memcpy(preencode_buf, &substream_id, 1);
	memcpy(preencode_buf+1, buf, len);
	len++;
	/* max overhead of COBS is 0.4%, plus the leading and trailing \0 bytes */
	size_t max_encoded_len = ceil(2 + 1.04*len);
	uint8_t encoded_message[max_encoded_len];
	/* add the leading \0, encoded message, and trailing \0 */
	encoded_message[0] = '\0';
	size_t encoded_len = cobs_encode(preencode_buf, len, encoded_message+1);
	encoded_message[encoded_len+1] = '\0';
	/* free pre-encoding buffer */
	free(preencode_buf);
	/* send, returning sent size to application */
	return send(socket->sock_fd, encoded_message, encoded_len+2, flags);
}

ssize_t send_message_time(hlywd_sock *socket, const void *buf, size_t len, int flags, uint16_t sequence_num, uint16_t depends_on, int lifetime_ms) {
	return send_message(socket, buf, len, flags);
}
#endif

/* Sends a message */
ssize_t send_message(hlywd_sock *socket, const void *buf, size_t len, int flags) {
	return send_message_sub(socket, buf, len, flags, 3);
}

/* Calculates maximum encoded message size */
size_t encoded_len(size_t len) {
	return ceil(2 + 1.04*len) + 2;
}

/* Receives a message */
ssize_t recv_message(hlywd_sock *socket, void *buf, size_t len, int flags, uint8_t *substream_id) {
	while (socket->message_count == 0) {
		uint8_t segment[1500+sizeof(tcp_seq)];
		tcp_seq sequence_num = 0;
		ssize_t segment_len = recv(socket->sock_fd, segment, 1500+sizeof(tcp_seq), flags);
		if (segment_len <= 0) {
			return segment_len;
		}
		#ifdef TCP_OODELIVERY
		if (segment_len >= sizeof(tcp_seq)) {
			memcpy(&sequence_num, segment+(segment_len-4), 4);
			segment_len -= 4;
		}
		#else
			sequence_num = socket->current_sequence_num;
			socket->current_sequence_num += segment_len;
		#endif
		parse_segment(socket, segment, segment_len, sequence_num);
	}
	return dequeue_message(socket, (uint8_t *)buf, substream_id);
}

/* Message queue functions */

/* Removes the message at the head of the message queue */
size_t dequeue_message(hlywd_sock *socket, uint8_t *buf, uint8_t *substream_id) {
	if (socket->message_count > 0) {
		memcpy(buf, socket->message_q_head->data, socket->message_q_head->len);
		memcpy(substream_id, &socket->message_q_head->substream_id, 1);
		message *dequeued_msg = socket->message_q_head;
		socket->message_q_head = dequeued_msg->next;
		size_t return_len = dequeued_msg->len;
		free(dequeued_msg->data-1);
		free(dequeued_msg);
		socket->message_count--;
		return return_len;
	} else {
		return -1;
	}
}

/* Adds a message to the end of the message queue */
int add_message(hlywd_sock *socket, uint8_t *data, size_t len) {
	message *new_message = (message *) malloc(sizeof(message));
	memcpy(&new_message->substream_id, data, 1);
	new_message->data = data+1;
	new_message->len = len-1;
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
	int message_region_start = 0;
	int message_region_end = segment_len-1;
	/* check for head */
	int head_end = 0;
	int head_len = 0;
	sparsebuffer_entry *sb_head_entry = NULL;
	sparsebuffer_entry *sb_tail_entry = NULL;
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
		size_t decoded_msg_len = cobs_decode(segment+msg_start+1, msg_end-msg_start-1, decoded_msg);
		memcpy(&substream_id, decoded_msg, 1);
		add_message(socket, decoded_msg, decoded_msg_len);
		current_byte++;
	}
}

/* Utility functions */

void print_data(uint8_t *data, size_t len) {
	printf("[");
	for (int i = 0; i < len; i++) {
		if (data[i] == '\0') {
			printf("0");
		} else {
			printf(".");
		}
	}
	printf("]\n");
}

/* Sparsebuffer functions */

sparsebuffer *new_sbuffer() {
	sparsebuffer *sb = (sparsebuffer *) malloc(sizeof(sparsebuffer));
	sb->head = NULL;
	sb->tail = NULL;
	return sb;
}

void print_sbuffer_entry(sparsebuffer_entry *sb_entry) {
	if (sb_entry == NULL) {
		printf("NULL\n");
	} else {
		printf("[%u, len: %zu] (", sb_entry->sequence_num, sb_entry->len);
		for (int i = 0; i < sb_entry->len; i++) {
			if (sb_entry->data[i] == '\0') {
				printf("0");
			} else {
				printf(".");
			}
		}
		printf(")\n");
	}
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
		print_sbuffer_entry(sb_entry);
		sb_entry = sb_entry->next;
	}
}

sparsebuffer_entry *add_entry(sparsebuffer *sb, tcp_seq sequence_num, size_t length, uint8_t *data) {
	/* is the sparsebuffer empty? */
	if (sb->head == NULL) {
		sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
		new_sbe->sequence_num = sequence_num;
		new_sbe->len = length;
		new_sbe->data = (uint8_t *) malloc(length);
		memcpy(new_sbe->data, data, length);
		new_sbe->next = NULL;
		new_sbe->prev = NULL;
		sb->head = new_sbe;
		sb->tail = new_sbe;
		return new_sbe;
	}
	/* no -- at least one entry */
	/* is this entry the new head? */
	if (sequence_num < sb->head->sequence_num) {
		/* yes, yes it is */
		if (sequence_num+length < sb->head->sequence_num) {
			sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
			new_sbe->sequence_num = sequence_num;
			new_sbe->len = length;
			new_sbe->data = (uint8_t *) malloc(length);
			memcpy(new_sbe->data, data, length);
			new_sbe->next = sb->head;
			new_sbe->prev = NULL;
			sb->head->prev = new_sbe;
			sb->head = new_sbe;
			return new_sbe;
		} else {
			sparsebuffer_entry *ends_in = sb->head;
			while (ends_in != NULL && sequence_num+length > ends_in->sequence_num+ends_in->len) {
				sparsebuffer_entry *next_cache = ends_in->next;
				destroy_sb_entry(ends_in);
				ends_in = next_cache;
			}
			if (ends_in == NULL) {
				sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
				new_sbe->sequence_num = sequence_num;
				new_sbe->len = length;
				new_sbe->data = (uint8_t *) malloc(length);
				memcpy(new_sbe->data, data, length);
				new_sbe->next = NULL;
				new_sbe->prev = NULL;
				sb->tail = new_sbe;
				sb->head = new_sbe;
				return new_sbe;
			} else if (sequence_num+length < ends_in->sequence_num) {
				sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
				new_sbe->sequence_num = sequence_num;
				new_sbe->len = length;
				new_sbe->data = (uint8_t *) malloc(length);
				memcpy(new_sbe->data, data, length);
				new_sbe->next = ends_in;
				new_sbe->prev = NULL;
				sb->head = new_sbe;
				ends_in->prev = new_sbe;
				return new_sbe;
			} else {
				sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
				new_sbe->sequence_num = sequence_num;
				new_sbe->len = (ends_in->sequence_num+ends_in->len)-sequence_num;
				new_sbe->data = (uint8_t *) malloc(length);
				memcpy(new_sbe->data, data, length);
				memcpy(new_sbe->data+length, ends_in->data, (ends_in->sequence_num+ends_in->len)-(sequence_num+length));
				new_sbe->next = ends_in->next;
				new_sbe->prev = NULL;
				sb->head = new_sbe;
				if (ends_in->next == NULL) {
					sb->tail = new_sbe;
				} else {
					ends_in->next->prev = new_sbe;
				}
				destroy_sb_entry(ends_in);
				return new_sbe;
			}
		}
	} else {
		/* no, it isn't. find the entry that this one starts in, or before */
		sparsebuffer_entry *starts_in = sb->head;
		while (starts_in != NULL && sequence_num > starts_in->sequence_num+starts_in->len) {
			starts_in = starts_in->next;
		}
		/* is this the new tail? */
		if (starts_in == NULL) {
			sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
			new_sbe->sequence_num = sequence_num;
			new_sbe->len = length;
			new_sbe->data = (uint8_t *) malloc(length);
			memcpy(new_sbe->data, data, length);
			new_sbe->next = NULL;
			new_sbe->prev = sb->tail;
			sb->tail->next = new_sbe;
			sb->tail = new_sbe;
			return new_sbe;
		}
		sparsebuffer_entry *new_sbe = (sparsebuffer_entry *) malloc(sizeof(sparsebuffer_entry));
		new_sbe->sequence_num = MIN(sequence_num, starts_in->sequence_num);
		/* find the entry we end inside, or after -- deleting entries along the way */
		sparsebuffer_entry *ends_in = starts_in;
		while (ends_in != NULL && sequence_num+length > ends_in->sequence_num+ends_in->len) {
			sparsebuffer_entry *next_cache = ends_in->next;
			if (ends_in != starts_in) {
				destroy_sb_entry(ends_in);
			}
			ends_in = next_cache;
		}
		if (ends_in == NULL) {
			new_sbe->len = (sequence_num+length)-new_sbe->sequence_num;
			new_sbe->data = (uint8_t *) malloc(new_sbe->len);
			if (new_sbe->sequence_num != sequence_num) {
				memcpy(new_sbe->data, starts_in->data, sequence_num-starts_in->sequence_num);
				memcpy(new_sbe->data+(sequence_num-starts_in->sequence_num), data, length);
				if (starts_in->prev == NULL) {
					sb->head = new_sbe;
					new_sbe->prev = NULL;
				} else {
					starts_in->prev->next = new_sbe;
					new_sbe->prev = starts_in->prev;
				}
			} else {
				memcpy(new_sbe->data, data, length);
				new_sbe->prev = starts_in->prev;
				starts_in->prev->next = new_sbe;
			}
			destroy_sb_entry(starts_in);
			new_sbe->next = NULL;
			sb->tail = new_sbe;
			return new_sbe;
		} else if (sequence_num+length < ends_in->sequence_num) {
			new_sbe->len = (sequence_num+length)-new_sbe->sequence_num;
			new_sbe->data = (uint8_t *) malloc(new_sbe->len);
			if (new_sbe->sequence_num != sequence_num) {
				memcpy(new_sbe->data, starts_in->data, sequence_num-starts_in->sequence_num);
				memcpy(new_sbe->data+(sequence_num-starts_in->sequence_num), data, length);
				if (starts_in->prev == NULL) {
					sb->head = new_sbe;
					new_sbe->prev = NULL;
				} else {
					starts_in->prev->next = new_sbe;
					new_sbe->prev = starts_in->prev;
				}
			} else {
				memcpy(new_sbe->data, data, length);
				new_sbe->prev = starts_in->prev;
				starts_in->prev->next = new_sbe;
			}
			destroy_sb_entry(starts_in);
			new_sbe->next = ends_in;
			ends_in->prev = new_sbe;	
			return new_sbe;	
		} else {
			new_sbe->len = (ends_in->sequence_num+ends_in->len)-new_sbe->sequence_num;
			new_sbe->data = (uint8_t *) malloc(new_sbe->len);
			if (new_sbe->sequence_num != sequence_num) {
				memcpy(new_sbe->data, starts_in->data, sequence_num-starts_in->sequence_num);
				memcpy(new_sbe->data+(sequence_num-starts_in->sequence_num), data, length);
				memcpy(new_sbe->data+(sequence_num-starts_in->sequence_num)+length, ends_in->data+((sequence_num+length)-ends_in->sequence_num), (ends_in->sequence_num+ends_in->len)-(sequence_num+length));
				if (starts_in->prev == NULL) {
					sb->head = new_sbe;
					new_sbe->prev = NULL;
				} else {
					starts_in->prev->next = new_sbe;
					new_sbe->prev = starts_in->prev;
				}
			} else {
				memcpy(new_sbe->data, data, length);
				if ((ends_in->sequence_num+ends_in->len)-(sequence_num+length) > 0) {
					memcpy(new_sbe->data+length, ends_in->data+((sequence_num+length)-ends_in->sequence_num), (ends_in->sequence_num+ends_in->len)-(sequence_num+length));
				}
				new_sbe->prev = starts_in->prev;
				if (starts_in->prev == NULL) {
					sb->head = new_sbe;
				} else {
					starts_in->prev->next = new_sbe;
				}
			}
			destroy_sb_entry(starts_in);
			new_sbe->next = ends_in->next;
			if (ends_in->next == NULL) {
				sb->tail = new_sbe;
			} else {
				ends_in->next->prev = new_sbe;
			}
			return new_sbe;
		}
	}
	return NULL;
}
