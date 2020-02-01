#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"
#include "crc.h"
#include "sp.h"

#define INITIAL_SIZE (256)
#define MAX_CHAIN_LENGTH (8)

/**
 *  We need to keep keys and values
 */
typedef struct _hashmap_element
{
	char     *key;
	size_t   in_use;
	size_t   tbd;     //!< to be deleted
	any_t    data;
}hmap_item_t;

/* A hashmap has some maximum size and current size,
 * as well as the data to hold. */
typedef struct _hashmap_map
{
	size_t         table_size;
	size_t         size;
	hmap_item_t   *data;
}hashmap_map;


//---------------------------------------------------------
// services
//---------------------------------------------------------

//! destructor
static void __hmap_free(void *ptr)
{
	if(!ptr)
		return;
	hashmap_map* m = (hashmap_map*)ptr;
	if(!m)
		return;

	for(size_t i=0; i<m->table_size; ++i)
	{
		if(m->data[i].in_use)
		{
			sp_free(m->data[i].data);
			sp_free(m->data[i].key);
		}
	}
	sp_free(m->data);
}

//! Hashing function for a string
static uint32_t __hash_internal(hashmap_map* m,const char* keystring)
{
	if (!m || !keystring)
		return 0;

	uint32_t key = crc32_calculate((const uint8_t*)keystring, strlen(keystring));

	//! Robert Jenkins' 32 bit Mix Function
	key += (key << 12);
	key ^= (key >> 22);
	key += (key << 4);
	key ^= (key >> 9);
	key += (key << 10);
	key ^= (key >> 2);
	key += (key << 7);
	key ^= (key >> 12);

	key = (key >> 3) * 2654435761;   /// Knuth's Multiplicative Method
	return key % m->table_size;
}

//! Return the integer of the location in data to store the point to the item, or MAP_FULL.
static uint32_t __hash(map_t* in,const char* key)
{
	if(!in || !key)
		return 0;

	hashmap_map* m = (hashmap_map *)in;

	//! If full, return immediately
	if (m->size >= (m->table_size/2))
		return MAP_FULL;

	//! Find the best index
	uint32_t curr = __hash_internal(m, key);

	/* Linear probing */
	for(size_t i=0; i< MAX_CHAIN_LENGTH; ++i)
	{
		if(!m->data[curr].in_use)
			return curr;

		if(m->data[curr].in_use && !strcmp(m->data[curr].key,key))
			return curr;

		curr = (curr + 1) % m->table_size;
	}
	return MAP_FULL;
}

//! Doubles the size of the hashmap, and rehashes all the elements
static int __rehash(map_t* in)
{
	if(!in)
		return MAP_OMEM;

	hashmap_map *m = (hashmap_map*)in;
   
	//! Setup the new elements
	hmap_item_t* temp = sp_t_calloc(2*m->table_size*sizeof(hmap_item_t), NULL,"_hashMapItem_");
	if(!temp)
		return MAP_OMEM;

	//! Update the array
	hmap_item_t *curr    = m->data;
	m->data = temp;

	//! Update the size
	size_t old_size = m->table_size;

	m->table_size *= 2;
	m->size        = 0;

	//! Rehash the elements
	for(size_t i=0; i < old_size; ++i)
	{
		if(!curr[i].in_use)
			continue;

		int status = hashmap_put(m, curr[i].key, curr[i].data);
		if(status != MAP_OK)
			return status;
      
		sp_free(curr[i].data);
		sp_free(curr[i].key);
	}

	sp_free(curr);
	return MAP_OK;
}

//---------------------------------------------------------
// interface
//---------------------------------------------------------
map_t* hashmap_new(void)
{
	hashmap_map* m = sp_t_malloc(sizeof(hashmap_map),__hmap_free,"_hashMap_");
	do
	{
		if(!m)
			break;

		m->data = sp_t_calloc(INITIAL_SIZE*sizeof(hmap_item_t), NULL,"_hashMapItem_");
		if(!m->data)
			break;

		m->table_size = INITIAL_SIZE;
		m->size = 0;

		return m;
	} while(0);

	sp_free(m);
	return NULL;
}


int hashmap_put(map_t* in,char* key, any_t value)
{
	if(!in || !key || !value)
		return MAP_OMEM;

	hashmap_map* m = (hashmap_map *) in;

	//! Find a place to put our value
	int idx = __hash(in, key);
	while(MAP_FULL == idx)
	{
		if(MAP_OMEM == __rehash(in))
			return MAP_OMEM;

		idx = __hash(in,key);
	}

	/* Set the data */
	m->data[idx].data   = sp_dup(value);
	m->data[idx].key    = sp_strdup(key);
	m->data[idx].in_use = 1;
	m->data[idx].tbd    = 0;
	m->size++; 

	return MAP_OK;
}

int hashmap_put2(map_t* in,void* key, any_t value)
{
	if (key) {
		return hashmap_put(in,key,value);
	} else {
		char buf[19] = {0}; // 0x1122334455667788 + '\0'
		snprintf(buf, sizeof(buf), "%p", value);

		return hashmap_put(in, buf, value);
	}
}

int hashmap_get(map_t* in,const char* key, any_t *arg)
{
	if(!in || !key || !arg)
		return MAP_OMEM;

	hashmap_map* m    = (hashmap_map*)in;
	uint32_t     curr = __hash_internal(m, key);    /// Find data location

	//! Linear probing, if necessary
	for(size_t i=0; i<MAX_CHAIN_LENGTH; ++i)
	{
		if(m->data[curr].in_use)
		{
			if(!strcmp(m->data[curr].key,key))
			{
				*arg = sp_dup((m->data[curr].data));
				return MAP_OK;
			}
		}
		curr = (curr + 1) % m->table_size;
	}

	*arg = NULL;
	return MAP_MISSING;  /// Not found
}

int hashmap_has(map_t* in,const char* key)
{
	if(!in || !key)
		return 0;
   
	int   rv  = 0;
	any_t tmp = NULL;
   
	hashmap_get(in,key,&tmp);
	rv = !!tmp;
	sp_free(tmp);
	return rv;
}

int hashmap_remove(map_t* in,const char* key)
{
	if(!in || !key)
		return MAP_OMEM;

	hashmap_map* m    = (hashmap_map*)in;
	uint32_t     curr = __hash_internal(m, key);  /// Find key

	//! Linear probing, if necessary
	for(size_t i=0; i<MAX_CHAIN_LENGTH; ++i)
	{
		if(m->data[curr].in_use)
		{
			if (strcmp(m->data[curr].key,key)==0)
			{
				//! Blank out the fields
				m->data[curr].in_use = 0;
            
				sp_free(m->data[curr].data);
				sp_free(m->data[curr].key);
            
				m->data[curr].data = NULL;
				m->data[curr].key  = NULL;

				m->size--;       /// Reduce the size
				return MAP_OK;
			}
		}
		curr = (curr + 1) % m->table_size;
	}
	return MAP_MISSING;  /// Data not found
}

int hashmap_remove2(map_t *in, const any_t ptr)
{
	char buf[19] = {0}; // 0x1122334455667788 + '\0'
	snprintf(buf, sizeof(buf), "%p", ptr);

	return hashmap_remove(in, buf);
}

int hashmap_clear(map_t* in,PFdestruct destructor)
{
	if(!in)
		return MAP_OMEM;

	hashmap_map* m = (hashmap_map*)in;

   //! Linear iterating
	for(size_t i=0; i<m->table_size; ++i)
	{
		if(m->data[i].in_use)
		{
			//! Blank out the fields
			m->data[i].in_use = 0;

			if(destructor)
				destructor((any_t)m->data[i].data);
         
			sp_free(m->data[i].data);
			sp_free(m->data[i].key);

			m->data[i].data = NULL;
			m->data[i].key  = NULL;

			m->size--;       /// Reduce the size
		}
	}
	return MAP_OK;  /// Data not found
}

int hashmap_iterate(map_t* in, PFany f, any_t item)
{
	if(!in || !f || !item)
		return MAP_OMEM;

	hashmap_map* m = (hashmap_map*) in;

	//! On empty hashmap, return immediately
	if(hashmap_length(m) <= 0)
		return MAP_MISSING;	

	//! Linear probing
	for(size_t i=0; i<m->table_size; ++i)
	{
		if(m->data[i].in_use)
		{
			any_t data = (any_t)m->data[i].data;
			int status = f(item, data);

			if(status != MAP_OK)
				return status;
		}
	}
	return MAP_OK;
}


int hashmap_cleanByCondition(map_t* in, PFany f, any_t arg, PFdestruct destructor)
{
	if (!in || !f || !arg)
		return MAP_OMEM;

	hashmap_map* m = (hashmap_map*) in;

	//! On empty hashmap, return immediately
	if(hashmap_length(m) <= 0)
		return MAP_MISSING;	

	//! Linear probing
	for(size_t i=0; i<m->table_size; ++i)
	{
		if(m->data[i].in_use)
		{
			any_t data = (any_t)m->data[i].data;
			if(MAP_OK == f(arg, data))
			{
				//! Blank out the fields
				m->data[i].in_use = 0;

				if(destructor)
					destructor((any_t)m->data[i].data);

				sp_free(m->data[i].data);
				sp_free(m->data[i].key);

				m->data[i].data = NULL;
				m->data[i].key  = NULL;

				m->size--;       /// Reduce the size
			}
		}
	}
	return MAP_OK;
}

size_t hashmap_length(map_t* in)
{
	return (!in) ? 0 : (((hashmap_map*)in)->size);
}
