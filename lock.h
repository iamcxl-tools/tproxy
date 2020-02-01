/*
 * lock.h
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */

#ifndef LOCK_H_
#define LOCK_H_

#include <stdint.h>
#include <pthread.h>

struct mutex_type;
typedef struct mutex_type mtx_t;

struct mutex_type
{
	pthread_mutex_t     mtx;        //!<  IPC object
	char                name[32];   //!<  name for this object
	uint8_t             is_locked;  //!<  cache the state
};

mtx_t* mutex_allocate(void);
int mutex_init(mtx_t *mutex);
int mutex_destroy(mtx_t *mutex);
int mutex_lock(mtx_t *mutex);
int mutex_unlock(mtx_t *mutex);


#endif /* LOCK_H_ */
