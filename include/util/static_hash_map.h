
/** @file static_hash_map.h
 *
 *  @brief Hash map implementation with no internal dynamic memory
 *  allocation. The table is created at a fixed size. Separate
 *  chaining is used to deal with collisions. This implementation is
 *  not synchronized.
 *
 *  To create a struct static_hash_map_s structure, please include
 *  static_hash_map_struct.h. It contains the internal
 *  representation of this data structure. However, once an instance
 *  is created, use the functions provided below to manipulate the
 *  internal fields; this will keep the data structure in a consistant
 *  state.
 *
 *  @author Naju Mancheril
 *
 *  @bug None known.
 */
#ifndef _STATIC_HASH_MAP_H
#define _STATIC_HASH_MAP_H




/* exported datatypes */


/** @typedef static_hash_map_t
 *
 *  @brief This is our static hash table datatype. Modules that need
 *  access to the hash table representation should include
 *  static_hash_map_struct.h.
 */
typedef struct static_hash_map_s* static_hash_map_t;

/** @typedef static_hash_node_t
 *
 *  @brief This is a node in one of our hash table's bucket
 *  chains. Modules that need access to the hash table representation
 *  should include static_hash_map_struct.h.
 */
typedef struct static_hash_node_s* static_hash_node_t;




/* exported functions */

/* Avoiding the const keyword with data pointers since it makes it
   difficult to hand back objects. Anyone that removes a payload must
   also treat it as const. Using const also prevents us from allowing
   functors that modify payloads passed to map. */

void static_hash_node_init( static_hash_node_t node, void* key, void* value );

void static_hash_map_init(static_hash_map_t ht,
			  struct static_hash_node_s* table_entries,
			  size_t table_size,
			  size_t (*hf) (const void* key),
			  int    (*comparator) (const void* key1, const void* key2) );

void static_hash_map_insert(static_hash_map_t ht,
			    void* key, void* value,
			    static_hash_node_t node);

int static_hash_map_find(static_hash_map_t ht,
			 void* key,
			 void** value,
			 static_hash_node_t* node);

int static_hash_map_remove(static_hash_map_t ht,
			   void* key,
			   void** value,
			   static_hash_node_t* node);

void static_hash_map_cut(static_hash_map_t ht, static_hash_node_t node);




#endif


