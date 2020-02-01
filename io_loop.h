/*
 * io_loop.h
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */

#ifndef IO_LOOP_H_
#define IO_LOOP_H_

#include <sys/epoll.h>
#include <stdint.h>

#include "socket_context.h"

typedef void (*io_cb_fn) (uint32_t events, void *ctx);

int io_add_sock(int fd, uint32_t events, void *data);
int io_del_sock(int fd);
int io_mod_sock(int fd, uint32_t events, void *data);

int  io_loop_init(io_cb_fn io_handler, io_cb_fn timer_handler, int timeout);
void io_loop_run(void);
void io_loop_stop(void);


#endif /* IO_LOOP_H_ */
