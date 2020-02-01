/*
 * socket_context.h
 *
 *  Created on: Jan 28, 2020
 *      Author: vitaliy
 */

#ifndef SOCKET_CONTEXT_H_
#define SOCKET_CONTEXT_H_

typedef void (*destroy_cb)(void*);

typedef enum context_type {
	LISTEN_CTX,
	BRIDGE_CLI_CTX,
	BRIDGE_SRV_CTX,
	TYPES_NUM
} ctx_type_t;

static const char * const type_str[TYPES_NUM] = {
	"LISTEN CONTEXT",
	"BRIDGE CLI CONTEXT",
	"BRIDGE SRV CONTEXT"
};

struct context_struct;
typedef struct context_struct ctx_t;

struct context_struct {
	int          fd;
	ctx_type_t   type;
	void        *data;
	destroy_cb   cb;
	ctx_t       *peer;
};

ctx_t *context_create(int fd, ctx_type_t type, void *data, destroy_cb);
void context_set_peer(ctx_t *ctx, ctx_t *peer);

#endif /* SOCKET_CONTEXT_H_ */
