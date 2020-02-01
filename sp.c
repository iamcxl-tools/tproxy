/*
 * sp.c
 *
 *  Created on: Jan 27, 2020
 *      Author: vitaliy
 */


#include <stdlib.h>
#include <string.h>

#include "sp.h"
#include "lock.h"

static void sp_init(sp_t *p)
{
	p->magic       = SP_MAGIC;
	p->cnt         = 1;
	p->sz          = 0;
	p->destruct_fn = NULL;
	p->mutex       = NULL;
	p->name[0]     = '\0';
}

void* sp_t_malloc(size_t s, void* destruct_fn, const char *t)
{
	if (!s)
		return NULL;
	sp_t* p = malloc(s+OFFSET);
	if(!p)
		return NULL;

	sp_init(p);
	p->sz = s;
	p->destruct_fn = destruct_fn;

	sp_addtag(p->data,t);
	return p->data;
}

void* sp_malloc(size_t s)
{
	return sp_t_malloc(s, NULL, "_malloc_");
}

void* sp_import(size_t s,void *ptr)
{
	void *rv = sp_malloc(s);
	if(rv)
		memcpy(rv,ptr,s);
	return rv;
}

void* sp_t_calloc(size_t s, void* destruct_fn, const char *t)
{
	sp_t* p = calloc(1,s+OFFSET);
	if(!p)
		return NULL;

	sp_init(p);
	p->sz = s;
	p->destruct_fn = destruct_fn;

	sp_addtag(p->data,t);
	return p->data;
}

void* sp_calloc(size_t s)
{
	return sp_t_calloc(s, NULL, "_calloc_");
}

void* sp_realloc(void* p,size_t s)
{
	if (!p)
		return sp_malloc(s);

	sp_t *q = INTERNAL_OBJ(p);
	if (!q)
		return NULL;

	if (0 == s) {
		sp_free(q);
		return NULL;
	}

	if(q->sz>=s) {
		q->sz=s;
		return p;
	}

	sp_t *tmp = realloc(q,s+OFFSET);	// if success - 'q' will be freed
	if (tmp) {
		q = tmp;
		q->sz=s;
	}

	return q->data;	// but we still don't know was pointer reallocated or not
}

char* sp_t_strdup(const char *s,const char *t)
{
	if(!s)
		return NULL;

	size_t  sz = (1+strlen(s));
	sp_t   *p  = malloc(sz+OFFSET);
	if (!p)
		return NULL;

	sp_init(p);
	p->cnt=1;
	p->sz=sz;

	sp_addtag(p->data,t);
	return memcpy(p->data,s,sz);
}

char* sp_strdup(const char *s)
{
	return sp_t_strdup(s,"_strdup_");
}

void* sp_t_copy(size_t s,const void* q,const char *t)
{
	if ((!s) || (!q))
		return NULL;

	sp_t* p=malloc(s+OFFSET);
	if (!p)
		return NULL;

	sp_init(p);
	p->sz = s;

	sp_addtag(p->data,t);
	return memcpy(p->data,q,s);
}

void* sp_copy(size_t s,const void* q)
{
	return sp_t_copy(s,q,"_copy_");
}

void* sp_dup(void* s)
{
	sp_t *q=INTERNAL_OBJ((void*)s);
	if (!q)
		return NULL;

	if (!q->mutex)
		q->mutex = mutex_allocate();

	mutex_lock(q->mutex);
	q->cnt++;
	mutex_unlock(q->mutex);

	return (void*)s;
}

void* sp_free(void* p)
{
	if (!p)
		return NULL;
	sp_t *q = INTERNAL_OBJ(p);

	if (p && !q) {
		free(p);
		return NULL;
	}

	mutex_lock(q->mutex);
	if (q->cnt-- <= 1) {
		sp_free(q->mutex);
		q->mutex = NULL;
		if (q->destruct_fn != NULL) {
			q->destruct_fn(p);
		}
		free(q);
	} else {
		mutex_unlock(q->mutex);
	}
	return NULL;
}

size_t sp_addtag(void *obj, const char *t)
{
	sp_t *p = INTERNAL_OBJ(obj);

	if (!p || !t)
		return 0;

	size_t rv = 0;
	size_t sz = strlen(t);
	if ( (1 <= p->cnt) && (sz < (MAX_TAG_LENGTH-1)) ) {
		memcpy(p->name,t,sz);
		p->name[sz] = '\0';
		rv++;
	}

	return rv;
}

char* sp_t_gettag(void *obj, const char *t)
{
	sp_t *p = INTERNAL_OBJ(obj);
	if (!p)
		return 0;

	if (!p->name[0])
		return NULL;

	return sp_t_strdup(p->name,t);
}

char* sp_gettag(void *obj)
{
	return sp_t_gettag(obj, "_gettag_");
}

void* sp_clone(const void* s)
{
	sp_t *q = INTERNAL_OBJ((void *)s);
	if (!q)
		return NULL;

	sp_t *n = malloc(q->sz+OFFSET);
	if (!n)
		return NULL;

	sp_init(n);

	/* clone metadata */
	n->sz = q->sz;
	n->destruct_fn = q->destruct_fn;
	sp_addtag(n->data,q->name);

	/* clone data */
	memcpy(n->data,q->data,n->sz);

	return n->data;
}

size_t sp_getsize(void* s)
{
	sp_t *q=INTERNAL_OBJ(s);
	return q ? q->sz : 0;
}

size_t sp_getcount(void* s)
{
	sp_t *q=INTERNAL_OBJ(s);
	return q ? q->cnt : 0;
}
