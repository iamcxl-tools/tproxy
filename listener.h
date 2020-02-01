/*
 * listener.h
 *
 *  Created on: Jan 28, 2020
 *      Author: vitaliy
 */

#ifndef LISTENER_H_
#define LISTENER_H_

typedef struct listener_type
{
	int fd;
} listener_t;

listener_t *listener_create(unsigned short port);

#endif /* LISTENER_H_ */
