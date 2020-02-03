/*
 * main.c
 *
 *  Created on: Jan 25, 2020
 *      Author: vitaliy
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>

#include "logger.h"
#include "sp.h"
#include "io_loop.h"
#include "socket_context.h"
#include "listener.h"
#include "bridge.h"
#include "hashmap.h"

map_t *map_active   = NULL;
map_t *map_stopping = NULL;

// I/O type
#define READ_IO  1
#define WRITE_IO 2

// I/O operation
#define ENABLE_IO  1
#define DISABLE_IO 2

static int bridge_mod_io(ctx_t *ctx, int io_type, int io_op)
{
	uint32_t events = 0;

	if (!ctx || !ctx->data)
		return -1;

	bridge_t *br = (bridge_t*)ctx->data;
	socket_ctx_t *sock = (BRIDGE_CLI_CTX == ctx->type) ? &br->cli : &br->srv;

	do {
		// READ_IO
		if (READ_IO == io_type) {
			if (ENABLE_IO == io_op){
				if (IO_ENABLED == sock->read_state)
					break ; // already set

				sock->read_state = IO_ENABLED;
				events |= EPOLLIN;
			}
			else if (DISABLE_IO == io_op) {
				if (IO_DISABLED == sock->read_state)
					break; // already unset

				sock->read_state = IO_DISABLED;
				// EPOLLIN is not set in events
			}

			if (IO_ENABLED == sock->write_state)
				events |= EPOLLOUT; // restore write
		}

		// WRITE_IO
		if (WRITE_IO == io_type) {
			if (ENABLE_IO == io_op) {
				if (IO_ENABLED == sock->write_state)
					break; // already set

				sock->write_state = IO_ENABLED;
				events |= EPOLLOUT;
			}
			else if (DISABLE_IO == io_op) {
				if (IO_DISABLED == sock->write_state)
					break; // already unset

				sock->write_state = IO_DISABLED;
				// EPOLLOUT is not set in events
			}

			if (IO_ENABLED == sock->read_state)
				events |= EPOLLIN; // restore read
		}

		return io_mod_sock(sock->fd, events, (void*)ctx);
	} while(0);

	return 0;
}

static void activate_bridge(ctx_t *cli_ctx, ctx_t *srv_ctx)
{
	bridge_mod_io(cli_ctx, READ_IO,  ENABLE_IO);
	bridge_mod_io(cli_ctx, WRITE_IO, DISABLE_IO);
	bridge_mod_io(srv_ctx, READ_IO,  ENABLE_IO);
	bridge_mod_io(srv_ctx, WRITE_IO, DISABLE_IO);

	return;
}

static void deactivate_bridge_context(ctx_t *ctx)
{
	bridge_t *bridge = NULL;

	LOGGER_DBG( "__%s: ctx {%p}\n", __FUNCTION__, ctx);

	do {
		if (!ctx)
			break;

		bridge = (bridge_t*)ctx->data;
		if (!bridge)
			break;

		bridge_mod_io(ctx, READ_IO,  DISABLE_IO);
		bridge_mod_io(ctx, WRITE_IO, DISABLE_IO);
		io_del_sock(ctx->fd);
	} while(0);

	return;
}

static int deactivate_listener(ctx_t *ctx)
{
	int rc = -1;
	listener_t *listener = NULL;

	do {
		if (!ctx)
			break;

		listener = (listener_t*)ctx->data;
		if (!listener)
			break;

		io_del_sock(listener->fd);
		close(listener->fd);
	} while(0);

	return rc;
}

static void destroy_context_cb(void *ptr)
{
	if (!ptr)
		return;

	ctx_t *ctx = (ctx_t*)ptr;
	if (BRIDGE_CLI_CTX == ctx->type || BRIDGE_SRV_CTX == ctx->type)
		deactivate_bridge_context(ctx);
	else if (LISTEN_CTX == ctx->type)
		deactivate_listener(ctx);
}

static void handle_io_listener(uint32_t events, ctx_t *ctx)
{
	int in_fd = -1;
	int err   = 1;
	bridge_t *bridge         = NULL;
	ctx_t    *bridge_srv_ctx = NULL;

	struct sockaddr_in cli_addr;
	socklen_t cli_addr_len = sizeof(cli_addr);

	do {
		LOGGER_DBG( "handle_io_listener: events {%"PRIu32"} fd {%d} data {%p}\n", events, ctx->fd, ctx->data);

		in_fd = accept4(ctx->fd, (struct sockaddr *)&cli_addr, &cli_addr_len, SOCK_NONBLOCK);
		if (in_fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return;

		bridge = bridge_create(in_fd);
		if (!bridge)
			break;

		bridge_srv_ctx = context_create(bridge->srv.fd, BRIDGE_SRV_CTX, bridge, destroy_context_cb);
		if (!bridge_srv_ctx)
			break;

		if (bridge_connect(bridge) < 0)
			break;

		if (BRIDGE_CONNECTING != bridge->state)
			break;

		bridge_mod_io(bridge_srv_ctx, WRITE_IO, ENABLE_IO);
		hashmap_put2(map_active, NULL, bridge_srv_ctx);

		err = 0;
	} while(0);

	if (bridge_srv_ctx)
		sp_free(bridge_srv_ctx);

	// unref bridge, it's still referenced by a context
	if (bridge)
		sp_free(bridge);
	else if (in_fd >= 0)
		close(in_fd);

	return;
}

static void handle_io_bridge_connecting(uint32_t events, ctx_t *ctx)
{
	bridge_t *bridge = (bridge_t *)ctx->data;
	ctx_t    *bridge_cli_ctx = NULL;
	ctx_t    *bridge_srv_ctx = ctx;

	struct sockaddr_in peer;
	int err = 0;
	int drop = 1;
	socklen_t len = 0;

	do {
		if ( !(events & EPOLLOUT) )
			break;

		len = sizeof(err);
		if (getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
			break;

		if (err) {
			LOGGER_DBG( "bridge {%p} failed to connect to srv: getsockopt SO_ERROR\n", bridge);
			break;
		}

		len = sizeof(peer);
		if (getpeername(ctx->fd, (struct sockaddr *)&peer, &len) < 0) {
			LOGGER_DBG( "bridge {%p} failed to connect to srv: getpeername error\n", bridge);
			break;
		}

		bridge_set_state(bridge, BRIDGE_ACTIVE);

		bridge_cli_ctx = context_create(bridge->cli.fd, BRIDGE_CLI_CTX, bridge, destroy_context_cb);
		if (!bridge_cli_ctx)
			break;

		hashmap_put2(map_active, NULL, bridge_cli_ctx);

		context_set_peer(bridge_cli_ctx, bridge_srv_ctx);
		context_set_peer(bridge_srv_ctx, bridge_cli_ctx);
		activate_bridge(bridge_cli_ctx, bridge_srv_ctx);

		LOGGER_DBG("bridge {%p} has been activated\n", bridge);

		drop = 0;
	} while(0);

	if (bridge_cli_ctx)
		sp_free(bridge_cli_ctx);

	if (drop) {
		bridge_set_state(bridge, BRIDGE_STOPPING);
		hashmap_remove2(map_active, ctx);
	}
}

static int context_flush_queue(ctx_t *ctx)
{
	int drop = 0;

	bridge_t     *bridge = (bridge_t *)ctx->data;
	send_queue_t *queue  = (BRIDGE_CLI_CTX == ctx->type) ? bridge->cli.queue : bridge->srv.queue;

	while(1) {
		int done = 0;
		int n    = 0;

		if (queue_is_empty(queue))
			break;

		send_queue_node_t *node = queue_get_first(queue);
		if (!node)
			break;

		n = write(ctx->fd, node->buf + node->drained, node->len - node->drained);
		if (n < 0) {
			if (errno != EAGAIN && errno != EINTR) {
				LOGGER_DBG( "write error to fd {%d} ctx {%p} bridge {%p}\n", ctx->fd, ctx, bridge);
				drop++;
			} else {
				done++;
			}
		} else {
			node->drained += n;
			if (node->drained == node->len)
				queue_del_first(queue);
		}

		if (node)
			sp_free(node);

		if (done)
			break;
	}

	if (drop)
		return -1;

	return 0;
}

static void adjust_io(bridge_t *br, ctx_t *cli_ctx, ctx_t *srv_ctx)
{
	LOGGER_DBG( "ctx {%p} bridge {%p} cli queue {%s} srv queue {%s}\n",
	                 ctx, br,
					 (queue_is_full(br->cli.queue)) ? "FULL" : "NOT FULL",
					 (queue_is_full(br->cli.queue)) ? "FULL" : "NOT FULL");

	if (queue_is_full(br->cli.queue)) bridge_mod_io(srv_ctx, READ_IO, DISABLE_IO);
	else                              bridge_mod_io(srv_ctx, READ_IO, ENABLE_IO);

	if (queue_is_full(br->srv.queue)) bridge_mod_io(cli_ctx, READ_IO, DISABLE_IO);
	else                              bridge_mod_io(cli_ctx, READ_IO, ENABLE_IO);

	if (queue_is_empty(br->cli.queue)) bridge_mod_io(cli_ctx, WRITE_IO, DISABLE_IO);
	else                               bridge_mod_io(cli_ctx, WRITE_IO, ENABLE_IO);

	if (queue_is_empty(br->srv.queue)) bridge_mod_io(srv_ctx, WRITE_IO, DISABLE_IO);
	else                               bridge_mod_io(srv_ctx, WRITE_IO, ENABLE_IO);
}

static void handle_io_bridge_active(uint32_t events, ctx_t *ctx)
{
	int drop = 0;
	int eof  = 0;

	bridge_t     *bridge = (bridge_t *)ctx->data;
	ctx_t        *peer   = ctx->peer;
	send_queue_t *queue  = NULL;

	if (events & EPOLLERR || events & EPOLLHUP) {
		if (BRIDGE_CLI_CTX == ctx->type) LOGGER_DBG( "connection closed from cli fd {%d} bridge {%p}\n", ctx->fd, bridge);
		if (BRIDGE_SRV_CTX == ctx->type) LOGGER_DBG( "connection closed from srv fd {%d} bridge {%p}\n", ctx->fd, bridge);
		drop++;
	}

	if (events & EPOLLIN) {
		queue = (BRIDGE_CLI_CTX == ctx->type) ? bridge->srv.queue : bridge->cli.queue;

		while(1) {
			char buf[QUEUE_SIZE];
			ssize_t n = read(ctx->fd, buf, sizeof(buf));
			if (!n) {
				LOGGER_DBG( "remote peer {%d} has closed its writing end, bridge {%p}\n", ctx->fd, bridge);
				eof++;
				break;
			} else if (n < 0) {
				if (errno == EAGAIN || errno == EINTR)
					break;

				LOGGER_DBG( "read error from fd {%d} ctx {%p} bridge {%p}\n", ctx->fd, ctx, bridge);
				drop++;
			} else {
				queue_enqueue(queue, buf, n);
				if (queue_is_full(queue))
					break;
			}
		}
	}

	if (events & EPOLLOUT) {
		if (context_flush_queue(ctx) < 0)
			drop++;
	}

	ctx_t *cli_ctx = (BRIDGE_CLI_CTX == ctx->type) ? ctx : ctx->peer;
	ctx_t *srv_ctx = (BRIDGE_SRV_CTX == ctx->type) ? ctx : ctx->peer;

	adjust_io(bridge, cli_ctx, srv_ctx);

	if (eof) {
		socket_ctx_t *read_socket = (BRIDGE_CLI_CTX == ctx->type) ? &bridge->cli : &bridge->srv;
		socket_ctx_t *peer_socket = (BRIDGE_CLI_CTX == ctx->type) ? &bridge->srv : &bridge->cli;

		// both queues are empty, close writing end
		if (queue_is_empty(bridge->cli.queue) && queue_is_empty(bridge->srv.queue))
			shutdown(peer_socket->fd, SHUT_WR);

		LOGGER_DBG( "=== bridge {%p} ctx {%p <-> %s} EOF\n", bridge, ctx, type_str[ctx->type]);

		read_socket->eof++;

		/* Stop reading from socket after EOF */
		bridge_mod_io(ctx, READ_IO, DISABLE_IO);

		/* Put into closing map to be cleaned up by timer */
		bridge_set_state(bridge, BRIDGE_STOPPING);
		hashmap_put2(map_stopping, NULL, ctx);
		hashmap_put2(map_stopping, NULL, peer);

		/* Delete from active map */
		hashmap_remove2(map_active, ctx);
		hashmap_remove2(map_active, peer);
	}

	if (drop) {
		LOGGER_DBG( "_____removing bridge {%p} contexts ctx {%p} peer {%p}\n", bridge, ctx, peer);

		bridge_set_state(bridge, BRIDGE_STOPPING);
		hashmap_remove2(map_active, ctx);
		hashmap_remove2(map_active, peer);
	}

	return;
}

static void handle_io_bridge_stopping(uint32_t events, ctx_t *ctx)
{
	int drop = 0;
	int eof  = 0;
	bridge_t     *bridge = (bridge_t *)ctx->data;
	ctx_t        *peer   = ctx->peer;
	send_queue_t *queue  = NULL;

	LOGGER_DBG( "got an event {%"PRIu32"} from the bridge {%p} in BRIDGE_STOPPING state\n", events, ctx);

	if (events & EPOLLERR || events & EPOLLHUP) {
		LOGGER_DBG( "connection closed from %s fd {%d} bridge {%p}\n", type_str[ctx->type], ctx->fd, bridge);
		drop++;
	}

	if (events & EPOLLIN) {
		queue = (BRIDGE_CLI_CTX == ctx->type) ? bridge->srv.queue : bridge->cli.queue;

		while(1) {
			char buf[QUEUE_SIZE];
			ssize_t n = read(ctx->fd, buf, sizeof(buf));
			if (!n) {
				LOGGER_DBG( "remote peer {%d} has closed its writing end, bridge {%p}\n", ctx->fd, bridge);
				eof++;
				break;
			} else if (n < 0) {
				if (errno == EAGAIN || errno == EINTR)
					break;

				LOGGER_DBG( "read error from fd {%d} ctx {%p} bridge {%p}\n", ctx->fd, ctx, bridge);
				drop++;
			} else {
				// TODO: try to deliver instead of dropping
				break;
			}
		}
	}

	if (events & EPOLLOUT) {
		if (context_flush_queue(ctx) < 0)
			drop++;

		send_queue_t *queue  = (BRIDGE_CLI_CTX == ctx->type) ? bridge->cli.queue : bridge->srv.queue;
		if (queue_is_empty(queue))
			shutdown(ctx->fd, SHUT_WR);
	}

	if (!drop && eof) {
		LOGGER_DBG("EOF from the opposite side bridge {%p} ctx {%p} fd {%d}\n", bridge, ctx, ctx->fd);

		socket_ctx_t *peer_socket = (BRIDGE_CLI_CTX == ctx->type) ? &bridge->srv : &bridge->cli;

		if (peer_socket->eof)
			drop++;
		else
			LOGGER_DBG(" BUG: How did get there bridge {%p} ctx {%p} fd {%d}\n", bridge, ctx, ctx->fd);

		/* Stop reading from socket after EOF */
		bridge_mod_io(ctx, READ_IO, DISABLE_IO);
	}

	if (queue_is_empty(bridge->cli.queue) && queue_is_empty(bridge->srv.queue)) {
		LOGGER_DBG( "all pending data has been sent\n");
		drop++;
	}

	if (drop) {
		LOGGER_DBG( "_____removing bridge {%p} contexts ctx {%p} peer {%p}\n", bridge, ctx, peer);

		bridge_set_state(bridge, BRIDGE_STOPPED);

		hashmap_remove2(map_stopping, ctx);
		hashmap_remove2(map_stopping, peer);
	}
}

static void handle_io_bridge(uint32_t events, ctx_t *ctx)
{
	bridge_t *bridge = (bridge_t *)ctx->data;
	if (!bridge)
		return;

	switch (bridge->state) {
		case BRIDGE_CONNECTING:
			handle_io_bridge_connecting(events, ctx);
			break;
		case BRIDGE_ACTIVE:
			handle_io_bridge_active(events, ctx);
			break;
		case BRIDGE_STOPPING:
			handle_io_bridge_stopping(events, ctx);
			break;
		default:
			break;
	}

	return;
}

static void handle_io(uint32_t events, void *data)
{
	ctx_t *ctx = (ctx_t*)data;

	char events_str[128] = {0};
	snprintf(events_str, sizeof(events_str),
	         "{%s} {%s} {%s} {%s}",
	         (events & EPOLLIN)  ? "POLLIN"  : "-",
	         (events & EPOLLOUT) ? "POLLOUT" : "-",
	         (events & EPOLLERR) ? "POLLERR" : "-",
	         (events & EPOLLHUP) ? "POLLHUP" : "-");

	LOGGER_DBG( "handle_io: ctx {%p <-> %s} events %s fd {%d}\n", ctx, type_str[ctx->type], events_str, ctx->fd);

	if (!events || !data)
		return;

	if (LISTEN_CTX == ctx->type)
		handle_io_listener(events, ctx);
	if (BRIDGE_CLI_CTX == ctx->type || BRIDGE_SRV_CTX == ctx->type)
		handle_io_bridge(events, ctx);

	return;
}

static int check_bridge_timeout(any_t arg, any_t obj)
{
	ctx_t    *ctx    = NULL;
	bridge_t *bridge = NULL;
	time_t   *current = (time_t*)arg;

	ctx = (ctx_t*)obj;
	if (!ctx)
		return MAP_MISSING;

	bridge = (bridge_t*)ctx->data;
	if (*current - bridge->stopping >= STOPPING_TIMEOUT) {
		LOGGER_DBG( "bridge {%p} is staying in BRIDGE_STOPPING for too long, stop it\n", bridge);
		return MAP_OK;
	}

	return MAP_MISSING;
}

static void handle_timer(uint32_t events, void *ctx)
{
	time_t current = time(NULL);

	hashmap_cleanByCondition(map_stopping, check_bridge_timeout, &current, NULL);

	return;
}

int main(int ac, char **av)
{
	listener_t *listener = NULL;
	ctx_t      *listen_context = NULL;
	int rc = -1;

	signal(SIGPIPE, SIG_IGN);

	do {
		map_active = hashmap_new();
		if (!map_active)
			break;

		map_stopping = hashmap_new();
		if (!map_stopping)
			break;

		if (io_loop_init(handle_io, handle_timer, 1000) < 0) {
			LOGGER_DBG( "failed to init io_loop\n");
			break;
		}

		listener = listener_create(1025);
		if (!listener) {
			LOGGER_DBG( "failed to create listener\n");
			break;
		}

		LOGGER_DBG("Listener {%p ; fd => %d} created\n", listener, listener->fd);

		listen_context = context_create(listener->fd, LISTEN_CTX, listener, destroy_context_cb);
		if (!listen_context) {
			LOGGER_DBG( "failed to create listener context\n");
			break;
		}

		hashmap_put2(map_active, NULL, listen_context);

		io_add_sock(listener->fd, EPOLLIN, (void*)listen_context);
		io_loop_run();

		LOGGER_DBG( "io_loop finished\n");
	} while(0);

	if (listen_context)
		sp_free(listen_context);

	if (listener)
		sp_free(listener);

	if (map_active)
		sp_free(map_active);

	if (map_stopping)
		sp_free(map_stopping);

	return 0;
}
