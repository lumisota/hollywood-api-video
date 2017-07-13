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

#ifndef HOLLYWOOD_H
#define HOLLYWOOD_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include "cobs.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct message {
	uint8_t *data;
	size_t len;
	uint8_t substream_id;
	struct message *next;
} message;

typedef struct sparsebuffer_entry {
	tcp_seq sequence_num;
	size_t len;
	uint8_t *data;
	struct sparsebuffer_entry *next;
	struct sparsebuffer_entry *prev;
} sparsebuffer_entry;

typedef struct sparsebuffer {
	sparsebuffer_entry *head;
	sparsebuffer_entry *tail;
} sparsebuffer;

typedef struct hlywd_sock {
	int sock_fd;
	struct timespec playout_delay;

	/* Message queue */
	message *message_q_head;
	message *message_q_tail;
	int message_count;

	sparsebuffer *sb;
	
	/* Current sequence number, when OO_DELIVERY not enabled */
	tcp_seq current_sequence_num;

	int oo;
	int pr;
} hlywd_sock;

int hollywood_socket(int fd, hlywd_sock *socket, int oo, int pr);

void set_playout_delay(hlywd_sock *socket, int pd_ms);

ssize_t send_message_time(hlywd_sock *socket, const void *buf, size_t len, int flags, uint16_t sequence_num, uint16_t depends_on, int lifetime_ms);
ssize_t send_message_sub(hlywd_sock *socket, const void *buf, size_t len, int flags, uint8_t substream_id);
ssize_t send_message(hlywd_sock *socket, const void *buf, size_t len, int flags);

ssize_t recv_message(hlywd_sock *socket, void *buf, size_t len, int flags, uint8_t *substream_id, int timeout_s);

ssize_t send_message_time_sub(hlywd_sock *socket, const void *buf, size_t len, int flags, uint16_t sequence_num, uint16_t depends_on, int lifetime_ms, uint8_t substream_id);

#endif
