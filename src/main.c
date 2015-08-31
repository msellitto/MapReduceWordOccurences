#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>

#include "chash.h"

#define HASH_SIZE 5000 // number of rows (buckets) in the hash

/*
 *  Solution to mapReduce word occurences
 *  Author: Matt Sellitto
 *
 *  1. Reads input file into memory
 *  2. Divides up input by size (making corrections for word boundaries)
 *  3. Starts the specified number of threads to count the words
 *  4. Each work thread parses input to find words and sets the word count in a shared concurrent hashMap
 *  5. Initial thread waits for completion of work threads
 *  6. Initial thread gets all key & values pairs from shared hashMap and sorts them
 *  7. Initial thread prints out sorted key & value pairs to stdout
 *
 *
*/

void print_usage(void) {
    printf("Invalid usage!\n");
    printf("Usage: mapred_woc <filename> <num_threads>\n");
}

typedef struct s_thread_info {
    long thread_id;
    long start_idx;
    long end_idx;
    long last_idx;
    char *input;
    chash * hash;
} thread_info;

void *WorkThread(void *t_info);
void log_word(long tid, long s, long e, char * in); 

void chash_test(void); 

// compare two keys lexographically, used for quick sorting keys
int cmpFunc(const void *a, const void *b) {
    return strcmp( ((keyValPair *)a)->key, ((keyValPair *)b)->key );
}

int main(int argc, char *argv[])
{
    if(argc != 3) {
        print_usage();
        return 1;
    }

    long int li_num_threads = strtol(argv[2], NULL, 10);

    if(li_num_threads <= 0) {
        printf("Invalid num_threads specified: %s\n", argv[2]);
        printf("Must be in the range of 1 to %li\n", LONG_MAX);
        return 1;
    }

    uint32_t num_threads = li_num_threads;

    char * in_filename = argv[1];

    //printf("Attemping to run mapred on input file %s with %li threads...\n", in_filename, li_num_threads); 

    FILE *fp = fopen(argv[1], "r");

    if(fp == NULL) {
        printf("Error opening file: %s\n", argv[1]);
        return 1;
    }

    long lSize;
    char *file_buffer;

    fseek( fp , 0L , SEEK_END);
    lSize = ftell( fp );
    rewind( fp );

    //printf("%s file contains %li bytes of text\n", in_filename, lSize);

    if(lSize <= 0) {
        printf("%s does not contain any valid text data to process, exiting...\n", in_filename);
        fclose(fp);
        exit(1);
    }

    char *text = malloc(lSize);
    if(text == NULL) {
        printf("Could not allocate memory for text processing, exiting...\n");
        fclose(fp);
        exit(1);
    }

    if (1 != fread(text, lSize, 1, fp) ) {
        printf("Could not read from input file, exiting...\n");
        free(text);
        fclose(fp);
        exit(1);
    }

    fclose(fp);

    chash *word_hash = chash_create(HASH_SIZE);

    if(word_hash == NULL) {
        printf("ERROR: Could not create word hash. Exiting...\n");
        free(text);
        exit(1);
    }

    if(li_num_threads > lSize) {
        printf("input text file size > num_threads specified, reducing num_threads to match...");
        li_num_threads = lSize;
        printf("num_threads = %li\n", li_num_threads);
    }

    long start_idx = 0; // (inclusive)
    long end_idx = 0; // (inclusive)
    long last_idx = lSize - 1;

    thread_info tis[li_num_threads];
    pthread_t threads[li_num_threads];

    // launch work threads
    for(long thread_num = 0; thread_num < li_num_threads; thread_num++) {
        end_idx = start_idx + (lSize / li_num_threads) - 1;
    
        if(thread_num == (li_num_threads -1)) {
            end_idx = last_idx;
        }

        //printf("work thread : %li has start_idx %li and end_idx %li\n", thread_num, start_idx, end_idx);

        tis[thread_num].thread_id = thread_num;
        tis[thread_num].start_idx = start_idx;
        tis[thread_num].end_idx = end_idx;
        tis[thread_num].input = text;
        tis[thread_num].last_idx = last_idx;
        tis[thread_num].hash = word_hash;

        pthread_create(&threads[thread_num], NULL, WorkThread, (void *) &tis[thread_num]);

        start_idx = end_idx + 1;
    }

    // wait for all work threads to finish
    for(long thread_num = 0; thread_num < li_num_threads; thread_num++) {
        pthread_join(threads[thread_num], NULL);
    }

    //chash_print(word_hash);

    // get the key=>value pairs and sort them by key

    keyValPair *kvs;

    uint32_t numKVs = chash_getKeyVals(word_hash, &kvs);

    qsort(kvs, numKVs, sizeof(keyValPair), cmpFunc);

    // print out thes sorted key / value pairs to stdout

    for(uint32_t i = 0; i < numKVs; i++) {
        printf("%s=%u\n", kvs[i].key, kvs[i].val);
    }

    free(kvs);

    chash_destroy(word_hash);
    free(text);

    return 0;
}


// returns true if a character is part of a word
// returns false if part of a separator
inline bool isWordChar(char c) {
    return isalnum(c);
}

// main worker threads for counting word occurances
void *WorkThread(void *t_info)
{
    thread_info ti = * (thread_info *) t_info;
    long tid = ti.thread_id;
    long start_idx = ti.start_idx;
    long end_idx = ti.end_idx;
    long last_idx = ti.last_idx;
    char *input = ti.input;
    chash *word_hash = ti.hash;

    // first fix start_idx...

    // if starts in the middle of a word adjust start_idx...
    if(start_idx != 0 && isWordChar(input[start_idx]) && isWordChar(input[start_idx-1])) {
        
        for(; (start_idx < end_idx) && isWordChar(input[start_idx]) ; start_idx++) {}
        //printf("Thread %li adjusted start_idx to %li\n", tid, start_idx);
    }

    if( (start_idx == end_idx) && (start_idx != 0) && (isWordChar(input[start_idx])) && (isWordChar(input[start_idx-1])) ) {
        //printf("Thread %li has no work to do, returning...\n", tid);
    }

    // now we know input[start_idx] either is a separator or the start of a new word

    bool inWord = false;
    long word_start = 0;
    long word_end = 0;

    for(long curr_idx = start_idx; curr_idx <= last_idx; curr_idx++) {

        const char c = input[curr_idx];
        const bool isWordCh = isWordChar(c);

        if( (curr_idx > end_idx) && !inWord) {
            // finished with this threads work, stop loop
            break;
        }

        // start of a word
        if(!inWord && isWordCh) {
            inWord = true;
           // printf("Thread %li found word at idx %li starting with: %c (%i)\n", tid, curr_idx, c, c);
            word_start = curr_idx;
        }

        // word ending with a regular separator
        else if( inWord && (!isWordCh) ) {
            word_end = curr_idx - 1;
            //log_word(tid, word_start, word_end, input);
            bool res = chash_setAndInc(word_hash, &input[word_start], word_end-word_start+1);

            if(!res) {
                printf("ERROR: Thread %li failed to set or increment hash. Thread exiting...\n", tid);
                pthread_exit(NULL);
            }

            inWord = false;
        }

        // word ending on last index
        else if( inWord && (curr_idx == last_idx) ) {
            word_end = curr_idx;
            //log_word(tid, word_start, word_end, input);
            bool res = chash_setAndInc(word_hash, &input[word_start], word_end-word_start+1);

            if(!res) {
                printf("ERROR: Thread %li failed to set or increment hash. Thread exiting...\n", tid);
                pthread_exit(NULL);
            }

            inWord = false;
        }

    }

    pthread_exit(NULL);
}




void log_word(long tid, long s, long e, char * in) {
    
   printf("logging word starting from idx %li to %li\n", s, e);

    size_t word_len = e-s+1;
    char *word = malloc(word_len+1);
    memcpy(word, &in[s], word_len);
    word[word_len] = '\0';

    printf("Thread %li found word: ", tid);
    printf("%s\n", word);
    free(word);
}

void chash_test(void) {

    chash * hash = chash_create(256);

    chash_setAndInc(hash, "abcd", 4);

    chash_setAndInc(hash, "abcd", 4);

    chash_setAndInc(hash, "abc", 3);

    chash_setAndInc(hash, "abcd", 4);

    chash_print(hash);

    chash_destroy(hash);
}

