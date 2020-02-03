/*
 * logger.h
 *
 *  Created on: Feb 1, 2020
 *      Author: vitaliy
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef DEBUG
#define LOGGER_DBG(fmt, ...) fprintf(stderr,"DBG %s:%d %s(): " fmt, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#else
#define LOGGER_DBG(fmt, ...)
#endif

#define LOGGER_ERR(fmt, ...) fprintf(stderr, "ERR %s:%d %s(): " fmt, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#endif /* LOGGER_H_ */
