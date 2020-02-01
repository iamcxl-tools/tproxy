/*
 * send_queue.h
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */

#ifndef SEND_QUEUE_H_
#define SEND_QUEUE_H_

#include <sys/queue.h>

typedef struct send_queue_node_type {
	char *buf;
	size_t len;
	size_t drained;
	TAILQ_ENTRY(send_queue_node_type) list;
} send_queue_node_t;

typedef struct send_queue_type
{
	size_t size;
	size_t max_size;
	TAILQ_HEAD(queue, send_queue_node_type) head;
} send_queue_t;


send_queue_t *queue_create(size_t max_size);
int queue_enqueue(send_queue_t *this, char *buf, size_t len);
int queue_is_empty(send_queue_t *this);
int queue_is_full(send_queue_t *this);
send_queue_node_t * queue_get_first(send_queue_t *this);
int queue_del_first(send_queue_t *this);

#endif /* SEND_QUEUE_H_ */
