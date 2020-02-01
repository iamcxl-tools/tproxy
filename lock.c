/*
 * lock.c
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */

#include <string.h>
#include "lock.h"
#include "sp.h"


void _mutex_free(void *ptr)
{
	if(!ptr)
		return;

	mtx_t *m = (mtx_t*)ptr;

	if (m->is_locked)
		mutex_unlock(m);

	mutex_destroy(m);
}

mtx_t* mutex_allocate(void)
{
	mtx_t *mutex = sp_t_calloc(sizeof(mtx_t),_mutex_free,"_mutex_");

	do {
		if (!mutex)
			break;
		if (mutex_init(mutex))
			break;

		return mutex;
	} while(0);

	sp_free(mutex);
	return NULL;
}

int mutex_init(mtx_t *m)
{
	if (!m)
		return -1;

	pthread_mutexattr_t  attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);

	int res = pthread_mutex_init(&m->mtx, &attr);
	pthread_mutexattr_destroy(&attr);

	if (!res) {
		m->name[0]   = '\0';
		m->is_locked = 0;
	}

	return res;
}

int mutex_destroy(mtx_t *m)
{
	if (!m)
		return 0;

	return pthread_mutex_destroy(&m->mtx);
}

int mutex_lock(mtx_t *m)
{
	if (!m)
		return 0;

	int res = pthread_mutex_lock(&m->mtx);
	if (!res) {
		m->is_locked++;
	}

	return res;
}

int mutex_unlock(mtx_t *m)
{
	if (!m)
		return 0;

	int res = pthread_mutex_unlock(&m->mtx);
	if (!res) {
		m->is_locked--;
	}
	return res;
}
