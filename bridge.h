/*
 * bridge.h
 *
 *  Created on: Jan 28, 2020
 *      Author: vitaliy
 */

#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <netinet/in.h>
#include <time.h>

#include "send_queue.h"

#define STOPPING_TIMEOUT 30
#define QUEUE_SIZE 32*1024

typedef enum bridge_state_type
{
	BRIDGE_NEW = 1,
	BRIDGE_CONNECTING,
	BRIDGE_ACTIVE,
	BRIDGE_STOPPING,
	BRIDGE_STOPPED
} bridge_state_t;

typedef enum io_status_type
{
	IO_ENABLED = 1,
	IO_DISABLED
} io_status_t;

typedef struct socket_context
{
	int fd;
	io_status_t read_state;
	io_status_t write_state;
	struct sockaddr_in sa;
	send_queue_t *queue;
	int eof;
} socket_ctx_t;

typedef struct bridge_type
{
	socket_ctx_t cli;
	socket_ctx_t srv;
	bridge_state_t state;
	time_t created;
	time_t connected;
	time_t stopping;
} bridge_t;

bridge_t *bridge_create(int cli_fd);
int bridge_connect(bridge_t *this);
void bridge_set_state(bridge_t *this, bridge_state_t state);

#endif /* BRIDGE_H_ */
