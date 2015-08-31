#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "chash.h"



static uint32_t hash(char *key, size_t key_size);
static node * chash_create_node(char * key, size_t key_size, uint32_t hash, uint32_t init_val); 

// returns true if a given node is a match for the given key
// returns false otherwise
inline static bool isNodeMatchForKey(node *node, char *key, size_t key_size, uint32_t hashVal) {

        if(node == NULL || hashVal != node->hash || key == NULL || key_size != node->key_size) {
            return false;
        }

        if( memcmp(node->key, key, key_size) == 0) {
            return true;
        }

        return false;
}

// create a new concurrent hashmap with the given size rows (buckets)
// returns a point to the hash if successful and NULL otherwise
chash * chash_create(uint32_t size) {

    chash *pcHash = malloc(sizeof(chash));

    if(pcHash == NULL) { return NULL; }

    pcHash->rows = malloc(sizeof(node *) * size);

    if(pcHash->rows == NULL) {
        free(pcHash);
        return NULL;
    }

    pcHash->row_locks = malloc(sizeof(pthread_mutex_t) * size);

    if(pcHash->row_locks == NULL) {
        free(pcHash);
        free(pcHash->rows);
        return NULL;
    }

    pcHash->size = size;

    for(uint32_t i = 0; i < size; i++) {
        pcHash->rows[i] = NULL;
        pthread_mutex_init(&(pcHash->row_locks[i]), NULL);
    }

    return pcHash;
}

void chash_destroy(chash *pcHash) {
    
    if(pcHash == NULL) return;

    for(uint32_t i = 0; i < pcHash->size; i++) {
        
        node *curr_node = pcHash->rows[i];

        while(curr_node != NULL) {
            node *next_node = curr_node->next;
            if(curr_node->key != NULL) {
                free(curr_node->key);
            }
            free(curr_node);
            curr_node = next_node;
        }


        pthread_mutex_destroy(&(pcHash->row_locks[i]));
    }


    if(pcHash->row_locks != NULL) {
        free(pcHash->row_locks);
    }

    if(pcHash->rows != NULL) {
        free(pcHash->rows);
    }

    free(pcHash);
    
}


// atomically increment hash[value] (or set to 1 if not found)
// return true if set succeeds
// returns false otherwise (invalid key or key_size or memory allocation error)
bool chash_setAndInc(chash *pcHash, char *key, size_t key_size) {

    if(pcHash == NULL || key == NULL || key_size == 0) {
        return false;;
    }

    uint32_t hashVal = hash(key, key_size);

    uint32_t row = hashVal % pcHash->size;

    pthread_mutex_lock(&(pcHash->row_locks[row]));

    node *last_node = NULL;
    node *curr_node = pcHash->rows[row];
    
    bool found_match = false;

    // if row is empty just add it to the beginning of the list
    if(pcHash->rows[row] == NULL) {
        pcHash->rows[row] = chash_create_node(key, key_size, hashVal, 1);
        if(pcHash->rows[row] == NULL) {
            return false;
        }
    }

    // else search the row for a key match
    else {

        // attempt to find a matching node in the row    
        while( curr_node != NULL && !(found_match = isNodeMatchForKey(curr_node, key, key_size, hashVal)) ) {
            last_node = curr_node;
            curr_node = curr_node->next;
        }

        // found it, just increment value
        if(curr_node != NULL && found_match) {
            curr_node->val++;
        }

        // else create a new node and add it to the row initialized with a value of 1
        else {
            node *new_node = chash_create_node(key, key_size, hashVal, 1);
            if(new_node == NULL) {
                return false;
            }
            last_node->next = new_node;
        }

    }

    pthread_mutex_unlock(&(pcHash->row_locks[row]));

    return true;
}

static node * chash_create_node(char * key, size_t key_size, uint32_t hash, uint32_t init_val) {
    
    node * retNode = malloc(sizeof(node));
   
    if(retNode == NULL) {
        return retNode;
    }

    retNode->key = malloc(key_size + 1); // allocate for key_size + 1 for additional null terminator
    memcpy(retNode->key, key, key_size);
    retNode->key[key_size] = '\0';
    retNode->hash = hash;
    retNode->next = NULL;
    retNode->key_size = key_size;
    retNode->val = init_val;

    return retNode;
}



// compute the hash for a given key and keysize
// using Jenkins hashmap described here: https://en.wikipedia.org/wiki/Jenkins_hash_function
static uint32_t hash(char *key, size_t len) {
   
    uint32_t hash, i;
    for(hash = i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}




void chash_print(chash *pcHash) {
    
    if(pcHash == NULL) return;

    printf("cHash has size of %u rows\n", pcHash->size);

    for(uint32_t i = 0; i < pcHash->size; i++) {
        node *curr_node = pcHash->rows[i];

        while(curr_node != NULL) {
            node *next_node = curr_node->next;

            printf("Row %u : Key = %s, val = %u , size = %zu hash = %u\n", i, curr_node->key, curr_node->val, curr_node->key_size, curr_node->hash);

            curr_node = next_node;
        }

    }

}


uint32_t chash_getKeyVals(chash *pcHash, keyValPair **kv) {
    
    if(pcHash == NULL) return 0;

    uint32_t numKVs = 0;

    for(uint32_t i = 0; i < pcHash->size; i++) {
        node *curr_node = pcHash->rows[i];

        while(curr_node != NULL) {
            node *next_node = curr_node->next;
            numKVs++;
            curr_node = next_node;
        }

    }

    //printf("numKVs = %u\n", numKVs);

    if(numKVs > 0) {

        uint32_t kv_idx = 0;

        keyValPair *kvps = malloc(sizeof(keyValPair) * numKVs);
        *kv = kvps;

        for(uint32_t i = 0; i < pcHash->size; i++) {
            node *curr_node = pcHash->rows[i];

            while(curr_node != NULL) {

                //printf("kv_idx = %u\n", kv_idx);

                //printf("key = %s, val = %u\n", curr_node->key, curr_node->val);

                node *next_node = curr_node->next;
    
                kvps[kv_idx].key = curr_node->key;
                kvps[kv_idx].val = curr_node->val;


                kv_idx++;

                curr_node = next_node;
            }

        }
    }

    return numKVs;
}
