/*
 * socket_context.c
 *
 *  Created on: Jan 28, 2020
 *      Author: vitaliy
 */


#include "socket_context.h"
#include "sp.h"
#include "logger.h"

static void __ctx_destroy(void *ptr)
{
	ctx_t *obj = (ctx_t*)ptr;

	LOGGER( "__destroy_ctx(%p)\n", ptr);

	if (!obj)
		return;

	if (obj->cb)
		obj->cb((void*)obj);

	if (obj->peer) {
		context_set_peer(obj->peer, NULL);
		context_set_peer(obj, NULL);
	}

	if (obj->data)
		sp_free(obj->data);
}

ctx_t *context_create(int fd, ctx_type_t type, void *data, destroy_cb cb)
{
	ctx_t *rc = NULL;

	do {
		if (fd < 0)
			break;

		rc = sp_t_calloc(sizeof(ctx_t), __ctx_destroy, "_ctx_t_");
		if (!rc)
			break;

		rc->fd   = fd;
		rc->type = type;
		rc->data = sp_dup(data);
		rc->cb = cb;

		return rc;
	} while(0);

	LOGGER( "failed to create context fd {%d} type {%s}\n", fd, type_str[type]);

	return NULL;
}

void context_set_peer(ctx_t *ctx, ctx_t *peer)
{
	if (!ctx)
		return;

	ctx->peer = peer;
}
