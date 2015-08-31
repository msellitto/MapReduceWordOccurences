
#ifndef _chash_h
#define _chash_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>


// a concurrent generic key to uint hashmap
// only needs to support a set or increment function for word counting
// but could be extended to support generic get and set as well

typedef struct s_node {
    char * key; // stores key with an extra null terminator
    size_t key_size; // size of key (not counting null termintor)
    uint32_t val;
    uint32_t hash;
    struct s_node *next;
} node;

typedef struct s_chash {
    uint32_t size;     
    node **rows;
    pthread_mutex_t *row_locks;
} chash;

typedef struct s_keyValPair {
    char *key;
    uint32_t val;
} keyValPair;


// create the hashmap
chash * chash_create(uint32_t size);

// destroy the hashmap
void chash_destroy(chash * pcHash);

// concurrently set ans increment the 
bool chash_setAndInc(chash *pcHash, char *key, size_t key_size); 

// prints out the contents of the hashmap (should not be used with any other methods that modify the hashmap
void chash_print(chash *pcHash);

// used to get a list of all key=>value pairs in the hashmap, should not be used with any other methods that modify the hashmap
uint32_t chash_getKeyVals(chash *pcHash, keyValPair **kv); 

#endif

