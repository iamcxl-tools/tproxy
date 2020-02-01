/*
 * socket_utils.c
 *
 *  Created on: Jan 28, 2020
 *      Author: vitaliy
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "socket_utils.h"

int configure_socket(int fd) {
	int enable = 1;
	int rc = -1;

	do {
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
			break;

		if (setsockopt(fd, SOL_IP, IP_TRANSPARENT, &enable, sizeof(enable)) < 0)
			break;

		rc = 0;
	} while(0);

	return rc;
}
