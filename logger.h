/*
 * logger.h
 *
 *  Created on: Feb 1, 2020
 *      Author: vitaliy
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef DEBUG
#define LOGGER(fmt, ...) fprintf(stderr,(fmt), __VA_ARGS__)
#else
#define LOGGER(fmt, ...)
#endif

#endif /* LOGGER_H_ */
