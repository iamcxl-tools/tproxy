/*
 * bridge.c
 *
 *  Created on: Jan 29, 2020
 *      Author: vitaliy
 */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "bridge.h"
#include "logger.h"
#include "sp.h"
#include "socket_utils.h"

static void __bridge_destroy(void *ptr)
{
	bridge_t *obj = (bridge_t*)ptr;

	LOGGER( "__bridge_destroy(%p)\n", ptr);

	if (!obj)
		return;

	if (obj->cli.fd >= 0)
		close(obj->cli.fd);

	if (obj->srv.fd >= 0)
		close(obj->srv.fd);

	if (obj->cli.queue)
		sp_free(obj->cli.queue);

	if (obj->srv.queue)
		sp_free(obj->srv.queue);
}

bridge_t *bridge_create(int cli_fd)
{
	bridge_t *rc = NULL;

	struct sockaddr_in cli_addr;
	struct sockaddr_in srv_addr;

	socklen_t cli_addr_len = sizeof(cli_addr);
	socklen_t srv_addr_len = sizeof(srv_addr);

	char cli_addr_buf[INET_ADDRSTRLEN] = {0};
	char srv_addr_buf[INET_ADDRSTRLEN] = {0};

	if (cli_fd < 0)
		return NULL;

	memset(&cli_addr, 0, sizeof(cli_addr));
	memset(&srv_addr, 0, sizeof(cli_addr));

	getpeername(cli_fd, (struct sockaddr *)&cli_addr, &cli_addr_len);
	getsockname(cli_fd, (struct sockaddr *)&srv_addr, &srv_addr_len);

	inet_ntop(AF_INET, &cli_addr.sin_addr, cli_addr_buf, sizeof(cli_addr_buf));
	inet_ntop(AF_INET, &srv_addr.sin_addr, srv_addr_buf, sizeof(srv_addr_buf));

	do {
		rc = sp_t_calloc(sizeof(bridge_t), __bridge_destroy, "_bridge_t_");
		if (!rc)
			break;

		rc->cli.sa = cli_addr;
		rc->srv.sa = srv_addr;

		rc->cli.fd = cli_fd;
		rc->srv.fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (rc->srv.fd < 0)
			break;

		if (configure_socket(rc->srv.fd) < 0)
			break;

		if (bind(rc->srv.fd,(struct sockaddr*)&cli_addr,sizeof(cli_addr)) < 0)
			break;

		rc->cli.queue = queue_create(QUEUE_SIZE);
		rc->srv.queue = queue_create(QUEUE_SIZE);

		if (!rc->cli.queue || !rc->srv.queue)
			break;

		bridge_set_state(rc, BRIDGE_NEW);

		LOGGER("New bridge {%p} {{%d <-> %p} <-> {%p <-> %d}}  {%s:%d <-> %s:%d}\n", rc,
				rc->cli.fd, rc->cli.queue, rc->srv.queue, rc->srv.fd,
		        cli_addr_buf, ntohs(cli_addr.sin_port),
		        srv_addr_buf, ntohs(srv_addr.sin_port));

		return rc;
	} while(0);

	if (rc)
		sp_free(rc);

	LOGGER( "failed to create bridge for fd {%d}\n", cli_fd);

	return NULL;
}

int bridge_connect(bridge_t *this)
{
	int rc = -1;

	do {
		if (!this)
			break;

		int n = connect(this->srv.fd, (struct sockaddr*)&this->srv.sa, sizeof(this->srv.sa));
		if (n < 0 && EINPROGRESS != errno)
			break;

		this->state = BRIDGE_CONNECTING;

		rc = 0;
	} while(0);

	if (rc < 0)
		bridge_set_state(this, BRIDGE_STOPPING);

	return rc;
}

void bridge_set_state(bridge_t *this, bridge_state_t state)
{
	switch (state) {
		case BRIDGE_NEW:
			this->state = BRIDGE_NEW;
			this->created = time(NULL);
			break;
		case BRIDGE_CONNECTING:
			this->state = BRIDGE_CONNECTING;
			break;
		case BRIDGE_ACTIVE:
			this->state = BRIDGE_ACTIVE;
			this->connected = time(NULL);
			break;
		case BRIDGE_STOPPING:
			this->state = BRIDGE_STOPPING;
			this->stopping = time(NULL);
			break;
		case BRIDGE_STOPPED:
			this->state = BRIDGE_STOPPED;
			break;
	}
}
