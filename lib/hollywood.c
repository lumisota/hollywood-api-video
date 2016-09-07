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

/* Message queue functions */
int add_message(hlywd_sock *socket, uint8_t *data, size_t len);
size_t dequeue_message(hlywd_sock *socket, uint8_t *buf, uint8_t *substream_id);

/* Fragment buffer functions */
int add_fragment(hlywd_sock *socket, tcp_seq sequence_num, uint8_t *data, size_t len);
void print_fragments(hlywd_sock *socket);
fragment *destroy_fragment(hlywd_sock *socket, fragment *target);
void scan_fragments(hlywd_sock *socket);

/* Segment parsing */
void parse_segment(hlywd_sock *socket, uint8_t *segment, size_t segment_len, tcp_seq sequence_num);

/* Creates a new Hollywood socket */
int hollywood_socket(int fd, hlywd_sock *socket) {
	int flag = 1;
	int debug_level;
	debug_level = 3;

	/* Disable Nagle's algorithm (TCP_NODELAY = 1) */
	int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

	/* Set debug level, if available */
	#ifdef TCP_HLYWD_DEBUG
	result = setsockopt(fd, IPPROTO_TCP, TCP_HLYWD_DEBUG, (char *) &debug_level, sizeof(int));
	#endif

	/* Enable out-of-order delivery, if available */
	#ifdef TCP_OODELIVERY
	result = setsockopt(fd, IPPROTO_TCP, TCP_OODELIVERY, (char *) &flag, sizeof(int));
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
	socket->fragment_buf_head = NULL;
	socket->fragment_buf_tail = NULL;
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
ssize_t send_message_time(hlywd_sock *socket, const void *buf, size_t len, int flags, uint16_t sequence_num, uint16_t depends_on, int framing_ms, int lifetime_ms) {
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

ssize_t send_message_time(hlywd_sock *socket, const void *buf, size_t len, int flags, uint16_t sequence_num, uint16_t depends_on, int framing_ms, int lifetime_ms) {
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
		ssize_t segment_len = recv(socket->sock_fd, segment, 1500+sizeof(tcp_seq), flags);
		if (segment_len <= 0) {
			return segment_len;
		}
		#ifdef TCP_OODELIVERY
		if (segment_len >= sizeof(tcp_seq)) {
			tcp_seq sequence_num;
			memcpy(&sequence_num, segment+(segment_len-sizeof(tcp_seq)), sizeof(tcp_seq));
			parse_segment(socket, segment, segment_len-4, sequence_num);
		}
		#else
		tcp_seq sequence_num = socket->current_sequence_num;
		socket->current_sequence_num += segment_len;
		parse_segment(socket, segment, segment_len, sequence_num);
		#endif
	}
	return dequeue_message(socket, buf, substream_id);
}

/* Parse an incoming segment */
void parse_segment(hlywd_sock *socket, uint8_t *segment, size_t segment_len, tcp_seq sequence_num) {
	int first_byte = 0;
	int last_byte = segment_len-1;
	/* check for head */
	int head_end = 0;
	while (head_end <= segment_len && segment[head_end] != '\0') {
		head_end++;
	}
	if (head_end > 0) {
		add_fragment(socket, sequence_num, segment, head_end+1);
		print_fragments(socket);
		first_byte = head_end + 1;
	}
	/* check for tail */
	int tail_start = segment_len-1;
	while (tail_start > head_end && segment[tail_start] != '\0') {
		tail_start--;
	}
	if (tail_start < segment_len-1) {
		add_fragment(socket, sequence_num+tail_start, segment+tail_start, segment_len-tail_start);
		print_fragments(socket);
		last_byte = tail_start-1;
	}
	/* check for full messages */
	int current_byte = first_byte;
	while (current_byte < last_byte) {
		int msg_start = current_byte;
		current_byte++;
		while (segment[current_byte] != '\0') {
			current_byte++;
		}
		int msg_end = current_byte;
		if (segment[msg_start] != 1) {
			uint8_t *decoded_message = (uint8_t *) malloc(msg_end-msg_start-1);
			size_t decoded_len = cobs_decode(segment+msg_start+1, msg_end-msg_start-1, decoded_message);
			add_message(socket, decoded_message, decoded_len);
		}
		current_byte++;
	}
	scan_fragments(socket);
}

/* Removes the message at the head of the message queue */
size_t dequeue_message(hlywd_sock *socket, uint8_t *buf, uint8_t *substream_id) {
	if (socket->message_count > 0) {
		memcpy(buf, socket->message_q_head->data, socket->message_q_head->len);
		memcpy(substream_id, &socket->message_q_head->substream_id, 1);
		message *dequeued_msg = socket->message_q_head;
		socket->message_q_head = dequeued_msg->next;
		size_t return_len = dequeued_msg->len;
		free(dequeued_msg->data);
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

/* Prints message fragment metadata */
void print_fragment(fragment *fragment) {
	if (fragment == NULL) {
		printf("--> NULL\n");
		return;
	}
	printf("--> %u (len %zu d %d).. ", fragment->sequence_num, fragment->len, fragment->dirty);
	int i;
	for (i = 0; i < fragment->len; i++) {
		printf("%02X ", fragment->data[i]);
	}
	printf("\n");
}

/* Prints all message fragments */
void print_fragments(hlywd_sock *socket) {
	fragment *current_fragment = socket->fragment_buf_head;
	while (current_fragment != NULL) {
		print_fragment(current_fragment);
		current_fragment = current_fragment->next;
	}
}

/* Scans message fragments for messages */
void scan_fragments(hlywd_sock *socket) {
	fragment *current_fragment = socket->fragment_buf_head;
	while (current_fragment != NULL) {
		if (current_fragment->dirty == 1) {
			uint8_t *segment = current_fragment->data;
			size_t segment_len = current_fragment->len;
			tcp_seq sequence_num = current_fragment->sequence_num;
			if (current_fragment->prev != NULL) {
				current_fragment->prev->next = current_fragment->next;
			} else {
				socket->fragment_buf_head = current_fragment->next;
			}
			if (current_fragment->next != NULL) {
				current_fragment->next->prev = current_fragment->prev;
			} else {
				socket->fragment_buf_tail = current_fragment->prev;
			}
			free(current_fragment);
			parse_segment(socket, segment, segment_len, sequence_num);
		} else if (current_fragment->ttl == 0) {
			current_fragment = destroy_fragment(socket, current_fragment);
			continue;
		} else {
			current_fragment->ttl--;
		}
		current_fragment = current_fragment->next;
	}
}

/* Removes target message fragment */
fragment *destroy_fragment(hlywd_sock *socket, fragment *target) {
	fragment *next_fragment = target->next;
	if (target->prev != NULL) {
		target->prev->next = target->next;
	} else {
		socket->fragment_buf_head = target->next;
	}
	if (target->next != NULL) {
		target->next->prev = target->prev;
	} else {
		socket->fragment_buf_tail = target->prev;
	}
	free(target);
	return next_fragment;
}

/* Adds message fragment to fragment buffer */
int add_fragment(hlywd_sock *socket, tcp_seq sequence_num, uint8_t *data, size_t len) {
	print_fragments(socket);
	fragment *left = socket->fragment_buf_head;
	while (left != NULL && left->next != NULL && sequence_num >= left->next->sequence_num) {
		left = left->next;
	}
	fragment *right = left;
	while (right != NULL && sequence_num+len >= right->sequence_num+right->len) {
		right = right->next;
	}
	if (left == NULL) {
		fragment *new_fragment = (fragment *) malloc(sizeof(fragment));
		uint8_t *data_copy = (uint8_t *) malloc(len);
		memcpy(data_copy, data, len);
		new_fragment->data = data_copy;
		new_fragment->sequence_num = sequence_num;
		new_fragment->dirty = 0;
		new_fragment->ttl = 10;
		new_fragment->len = len;
		new_fragment->next = NULL;
		new_fragment->prev = NULL;
		socket->fragment_buf_head = new_fragment;
		socket->fragment_buf_tail = new_fragment;
		return 1;
	}
	if (left == socket->fragment_buf_head && sequence_num < left->sequence_num) {
		if (right == NULL || sequence_num+len < right->sequence_num) {
			/* first, delete any fragments that are no longer needed */
			fragment *current_fragment = left->next;
			while (current_fragment != right) {
				fragment *next = current_fragment->next;
				free(current_fragment->data);
				free(current_fragment);
				current_fragment = next;
			}
			uint8_t *new_data = (uint8_t *) malloc(len);
			memcpy(new_data, data, len);
			free(left->data);
			left->sequence_num = sequence_num;
			left->data = new_data;
			left->len = len;
			left->next = right;
			left->dirty = 1;
			if (right != NULL) {
				right->prev = left;
			}
			return 1;
		} else {
			/* first, delete any fragments that are no longer needed */
			if (left != right) {
				fragment *current_fragment = left->next;
				while (current_fragment != right) {
					fragment *next = current_fragment->next;
					free(current_fragment->data);
					free(current_fragment);
					current_fragment = next;
				}
			}
			size_t new_len = right->sequence_num+right->len - sequence_num;
			uint8_t *new_data = (uint8_t *) malloc(new_len);
			memcpy(new_data, data, len);
			memcpy(new_data+len, right->data+(sequence_num+len-right->sequence_num), new_len-len);
			free(left->data);
			free(right->data);
			left->sequence_num = sequence_num;
			left->dirty = 1;
			left->data = new_data;
			left->len = new_len;
			if (right != NULL && right->next != NULL) {
				right->next->prev = left;
			}
			left->next = right->next;
			if (left != right) {
				free(right);
			}
			return 1;
		}
	}
	if (sequence_num > left->sequence_num+left->len && (right == NULL || sequence_num+len < right->sequence_num)) {
		/* first, delete any fragments that are no longer needed */
		fragment *current_fragment = left->next;
		while (current_fragment != right) {
			fragment *next = current_fragment->next;
			free(current_fragment->data);
			free(current_fragment);
			current_fragment = next;
		}
		fragment *new_fragment = (fragment *) malloc(sizeof(fragment));
		uint8_t *data_copy = (uint8_t *) malloc(len);
		memcpy(data_copy, data, len);
		new_fragment->data = data_copy;
		new_fragment->sequence_num = sequence_num;
		new_fragment->dirty = 0;
		new_fragment->ttl = 10;
		new_fragment->len = len;
		new_fragment->prev = left;
		new_fragment->next = right;
		left->next = new_fragment;
		if (right == NULL) {
			socket->fragment_buf_tail = new_fragment;
		}
		return 1;
	}
	if (sequence_num >= left->sequence_num && sequence_num <= left->sequence_num+left->len && (right == NULL || sequence_num+len < right->sequence_num)) {
		/* first, delete any fragments that are no longer needed */
		fragment *current_fragment = left->next;
		while (current_fragment != right) {
			fragment *next = current_fragment->next;
			free(current_fragment->data);
			free(current_fragment);
			current_fragment = next;
		}
		size_t new_len = (sequence_num+len)-left->sequence_num;
		uint8_t *new_data = (uint8_t *) malloc(new_len);
		memcpy(new_data, left->data, sequence_num-left->sequence_num);
		memcpy(new_data+(sequence_num-left->sequence_num), data, len);
		free(left->data);
		left->dirty = 1;
		left->data = new_data;
		left->len = new_len;
		return 1;
	}
	if (sequence_num >= left->sequence_num && sequence_num <= left->sequence_num+left->len && (right != NULL && sequence_num+len >= right->sequence_num)) {
		/* first, delete any fragments that are no longer needed */
		fragment *current_fragment = left->next;
		while (current_fragment != NULL && current_fragment != right) {
			fragment *next = current_fragment->next;
			free(current_fragment->data);
			free(current_fragment);
			current_fragment = next;
		}
		size_t new_len = (right->sequence_num+right->len)-(left->sequence_num);
		uint8_t *new_data = (uint8_t *) malloc(new_len);
		memcpy(new_data, left->data, sequence_num-left->sequence_num);
		memcpy(new_data+(sequence_num-left->sequence_num), data, len);
		memcpy(new_data+(sequence_num-left->sequence_num)+len, right->data+(sequence_num+len-right->sequence_num), (right->sequence_num+right->len)-(sequence_num+len));
		free(left->data);
		free(right->data);
		left->data = new_data;
		left->next = right->next;
		left->dirty = 1;
		left->len = new_len;
		if (right->next != NULL) {
			right->next->prev = left;
		}
		free(right);
		return 1;
	}
	if (sequence_num > left->sequence_num+left->len && (right != NULL && sequence_num+len >= right->sequence_num)) {
		/* first, delete any fragments that are no longer needed */
		fragment *current_fragment = left->next;
		while (current_fragment != right) {
			fragment *next = current_fragment->next;
			free(current_fragment->data);
			free(current_fragment);
			current_fragment = next;
		}
		size_t new_len = (right->sequence_num+right->len)-sequence_num;
		uint8_t *new_data = (uint8_t *) malloc(new_len);
		memcpy(new_data, data, len);
		memcpy(new_data+len, right->data+(sequence_num+len-right->sequence_num), new_len-len);
		free(right->data);
		right->data = new_data;
		right->dirty = 1;
		right->sequence_num = sequence_num;
		right->len = new_len;
		return 1;
	}
	return 0;
}
