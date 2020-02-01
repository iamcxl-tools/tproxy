/*
 * listener.c
 *
 *  Created on: Jan 28, 2020
 *      Author: vitaliy
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "listener.h"
#include "socket_utils.h"
#include "sp.h"

static void __listener_destroy(void *ptr)
{
	listener_t *obj = (listener_t*)ptr;

	if (!obj)
		return;

	if (obj->fd)
		close(obj->fd);
}

listener_t *listener_create(unsigned short port)
{
	listener_t *rc = NULL;
	struct sockaddr_in listen_addr;

	do {
		if (!port)
			break;

		rc = sp_t_calloc(sizeof(listener_t), __listener_destroy, "listener_t");
		if (!rc)
			break;

		rc->fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (rc->fd < 0)
			break;

		if (configure_socket(rc->fd) < 0)
			break;

		memset(&listen_addr,0,sizeof(listen_addr));
		listen_addr.sin_family = AF_INET;
		listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		listen_addr.sin_port = htons(port);

		if (bind(rc->fd,(struct sockaddr*)&listen_addr,sizeof(listen_addr)) < 0)
			break;

		if (listen(rc->fd, 100) < 0)
			break;

		return rc;
	} while(0);

	if (rc)
		sp_free(rc);

	return NULL;
}
