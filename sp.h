/*
 * sp.h
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */

#ifndef SP_H_
#define SP_H_

#define MAX_TAG_LENGTH  48

#include <stddef.h>
#include <stdio.h>

#include "lock.h"

typedef void (*sp_destruct_fn)(void *);

struct smart_pointer_struct;
typedef struct smart_pointer_struct sp_t;

struct smart_pointer_struct
{
	uint32_t        magic;			//!< binary identifier
	size_t          cnt;			//!< reference counter
	size_t          sz;				//!< bytes allocated
	sp_destruct_fn  destruct_fn;	//!< destructor function
	mtx_t           *mutex;			//!< mutex
	char            name[32];		//!< ascii identifier
	char            data[];			//!< dynamic data buffer
};

#define SP_MAGIC 0xF0CA4400
#define OFFSET offsetof(sp_t,data)

static inline sp_t *INTERNAL_OBJ(void *user_data)
{
	if (!user_data) {
		return NULL;
	}

	sp_t *p = user_data-OFFSET;
	if (SP_MAGIC != p->magic) {
		fprintf(stderr, "ERROR: bad magic number for pointer %p.\n", user_data);
		return NULL;
	}

	return p;
}

#define EXTERNAL_OBJ(_p)	((_p) == NULL ? NULL : (_p)->data)

void* sp_t_malloc(size_t s, void* destruct_fn, const char *t);
void* sp_malloc(size_t s);
void* sp_import(size_t s,void *ptr);
void* sp_t_calloc(size_t s, void* destruct_fn, const char *t);
void* sp_calloc(size_t s);
void* sp_realloc(void *obj,size_t s);
char* sp_t_strdup(const char *s,const char *t);
char* sp_strdup(const char *s);
void* sp_t_copy(size_t s,const void *obj,const char *t);
void* sp_copy(size_t s,const void *obj);
void* sp_dup(void *obj);
void* sp_free(void *obj);
size_t sp_addtag(void *obj,const char *tag);
char* sp_t_gettag(void *obj,const char *tag);
char* sp_gettag(void *obj);
void* sp_clone(const void *obj);
size_t sp_getsize(void *obj);
size_t sp_getcount(void *obj);

#endif /* SP_H_ */
