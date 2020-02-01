/*
 * Generic hashmap manipulation functions
 *
 * Originally by Elliot C Back - http://elliottback.com/wp/hashmap-implementation-in-c/
 *
 * Modified by Pete Warden to fix a serious performance problem, support strings as keys
 * and removed thread synchronization - http://petewarden.typepad.com
 *
 */
#ifndef HASHMAP_H_
#define HASHMAP_H_

#include <stddef.h>

#define MAP_MISSING -3  /* No such element */
#define MAP_FULL    -2  /* Hashmap is full */
#define MAP_OMEM    -1  /* Out of Memory */
#define MAP_OK       0  /* OK */

#define MAP_MATCH     -4      /* pattern has been found */
#define MAP_NO_MATCH  MAP_OK  /* continue iteration */

/*
 * any_t is a pointer.  This allows you to put arbitrary structures in
 * the hashmap.
 */
typedef void *any_t;

/*
 * PFany is a pointer to a function that can take two any_t arguments
 * and return an integer. Returns status code..
 */
typedef int (*PFany)(any_t, any_t);

/*
 * PFdestruct is a pointer to a function that can deinit element
 */
typedef void (*PFdestruct)(any_t);


/*
 * map_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how hashmaps are
 * represented.  They see and manipulate only map_t's.
 */
typedef void map_t;

/*
 * Return an empty hashmap. Returns NULL if empty.
*/
map_t* hashmap_new(void);

/*
 * Add an element to the hashmap. Return MAP_OK or MAP_OMEM.
 */
int hashmap_put(map_t* in,char* key, any_t value);

/*
 * Add an element to the hashmap. Return MAP_OK or MAP_OMEM.
 * If key is NULL uses value address converted to a string
 */
int hashmap_put2(map_t* in,void* key, const any_t value);

/*
 * Get an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
int hashmap_get(map_t* in,const char* key, any_t *arg);

/*
 * check for existence of element
 */
int hashmap_has(map_t* in,const char* key);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
int hashmap_remove(map_t* in,const char* key);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
int hashmap_remove2(map_t* in,const any_t value);

/*
 * Remove all elements from the hashmap. Return MAP_OK or error.
 */
int hashmap_clear(map_t* in,PFdestruct destructor);

/*
 * Iteratively call f with argument (item, data) for
 * each element data in the hashmap. The function must
 * return a map status code. If it returns anything other
 * than MAP_OK the traversal is terminated. f must
 * not reenter any hashmap functions, or deadlock may arise.
 */
int hashmap_iterate(map_t* in, PFany f, any_t item);

/*
 * Remove elements which satisfy the condition
 */
int hashmap_cleanByCondition(map_t* in, PFany f, any_t item, PFdestruct destructor);

/*
 * Get the current size of a hashmap
 */
size_t hashmap_length(map_t* in);


#endif

//-----------------------------------------------------------------------------
// eof 
//----------------------------------------------------------------------------- 
