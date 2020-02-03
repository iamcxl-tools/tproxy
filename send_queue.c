/*
 * send_queue.c
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */


#include <stddef.h>
#include <string.h>

#include "send_queue.h"
#include "sp.h"
#include "logger.h"

static void __queue_destroy(void *ptr)
{
	send_queue_t *queue = (send_queue_t *)ptr;

	if (!queue)
		return;

	LOGGER_DBG( "%s: %p\n", __FUNCTION__, queue);

	while (!queue_del_first(queue))
		;
}

static void __queue_node_destroy(void *ptr)
{
	send_queue_node_t *node = (send_queue_node_t*)ptr;
	if (!node)
		return;

	LOGGER_DBG( "%s: %p\n", __FUNCTION__, node);

	sp_free(node->buf);
}

send_queue_t *queue_create(size_t max_size)
{
	send_queue_t *rc = NULL;

	do {
		rc = sp_t_calloc(sizeof(send_queue_t), __queue_destroy, "send_queue_t");
		if (!rc)
			break;

		rc->size     = 0;
		rc->max_size = max_size;
		TAILQ_INIT(&rc->head);

		return rc;
	} while(0);

	if (rc)
		sp_free(rc);

	return NULL;
}

int queue_enqueue(send_queue_t *this, char *buf, size_t len)
{
	int rc = -1;
	send_queue_node_t *node = NULL;

	do {
		if (!this)
			break;

		if (!buf || !len)
			break;

		node = sp_t_calloc(sizeof(send_queue_node_t), __queue_node_destroy, "send_queue_node_t");
		if (!node)
			break;

		node->buf = sp_malloc(len);
		if (!node->buf)
			break;

		node->drained = 0;
		node->len     = len;
		memcpy(node->buf, buf, len);

		this->size += len;

		TAILQ_INSERT_TAIL(&this->head, node, list);
		rc = 0;
	} while(0);

	if (rc && node)
		sp_free(node);

	LOGGER_DBG( "___%s: this {%p} len {%d}, result {%s}\n", __FUNCTION__, this, len, (rc)?"error":"ok");

	return rc;
}

int queue_is_empty(send_queue_t *this)
{
	int rc = 0;

	do {
		if (!this)
			break;

		if (TAILQ_EMPTY(&this->head))
			rc++;
	} while(0);

	LOGGER_DBG( "___%s: this {%p}, result {%s}\n", __FUNCTION__, this, (rc)?"empty":"not empty");

	return rc;
}

int queue_is_full(send_queue_t *this)
{
	int rc = 0;

	do {
		if (!this)
			break;

		rc = (this->size >= this->max_size);
	} while(0);

	LOGGER_DBG( "___%s: this {%p}, result {%s}\n", __FUNCTION__, this, (rc)?"full":"not full");

	return rc;
}

send_queue_node_t * queue_get_first(send_queue_t *this)
{
	send_queue_node_t *node = NULL;

	LOGGER_DBG( "___%s: this\n", __FUNCTION__, this);

	if (!this)
		return NULL;

	node = sp_dup(TAILQ_FIRST(&this->head));

	LOGGER_DBG( "___%s: this {%p}, result {%p}\n", __FUNCTION__, this, node);

	return node;
}

int queue_del_first(send_queue_t *this)
{
	int rc = -1;

	LOGGER_DBG( "___%s: this {%p}\n", __FUNCTION__, this);

	do {
		if (!this)
			break;

		send_queue_node_t *node = (send_queue_node_t*)TAILQ_FIRST(&this->head);
		if (!node)
			break;

		TAILQ_REMOVE(&this->head, node, list);

		this->size -= node->len;

		sp_free(node);
		rc = 0;
	} while(0);

	return rc;
}
