/*
 * io_loop.c
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/epoll.h>

#include "io_loop.h"
#include "sp.h"
#include "logger.h"

static int efd = -1;
static io_cb_fn io_cb = NULL;
static io_cb_fn timer_cb = NULL;
static int timeout = 100;
static int force_exit = 0;

int io_loop_init(io_cb_fn io_handler,io_cb_fn timer_handler, int timeout)
{
	int rc = -1;

	do {
		if (!io_handler || !timer_handler) {
			LOGGER( "handlers are not set\n");
			break;
		}

		if (efd >= 0) {
			LOGGER( "io_loop has been already initialized\n");
			break;
		}

		efd = epoll_create1(0);
		if (efd < 0) {
			LOGGER( "epoll_create1() error: %s\n", strerror(errno));
			break;
		}

		io_cb    = io_handler;
		timer_cb = timer_handler;

		rc = 0;
	} while(0);

	return rc;
}

int io_add_sock(int fd, uint32_t events, void *data)
{
	return io_mod_sock(fd, events, data);
}

int io_mod_sock(int fd, uint32_t events, void *data)
{
	struct epoll_event event;
	int rc = -1;

	LOGGER( "__io_mod_sock: fd {%d} events {%s} {%s}\n", fd,
	                (events & EPOLLIN)  ? "POLLIN"  : "-",
	                (events & EPOLLOUT) ? "POLLOUT" : "-");

	do {
		if (efd < 0)
			break;

		event.events = events;
		event.data.ptr = data;

		rc = epoll_ctl(efd, EPOLL_CTL_MOD, fd, &event);
		if (rc < 0 && ENOENT == errno)
			rc = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
	} while(0);

	return rc;
}

int io_del_sock(int fd)
{
	LOGGER( "io_del_sock: fd {%d}\n", fd);

	return epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
}

void io_loop_run(void)
{
#define MAXEVENTS 100

	int n = 0, i = 0;
	struct epoll_event *events = NULL;

	events = sp_t_calloc(MAXEVENTS * sizeof(struct epoll_event), NULL, "_epoll_events_");
	if (!events)
		return;

	while (!force_exit) {
		timer_cb(0,NULL);

		n = epoll_wait(efd, events, MAXEVENTS, timeout);
		if (!n || (n < 0 && errno == EINTR))
			continue;

		if (n < 0) {
			LOGGER( "epoll_wait failed: %s\n", strerror(errno));
			break;
		}

		for (i = 0; i < n; i++) {
			if (!events[i].events)
				continue;
			io_cb(events[i].events, events[i].data.ptr);
		}
	}

	close(efd);
	efd = -1;

	return;
}

void io_loop_stop(void)
{
	force_exit++;
}
