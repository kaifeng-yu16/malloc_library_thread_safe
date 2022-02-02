#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// structs & typedef
typedef struct meta_data {
  size_t size;
  size_t is_used;
  struct meta_data * prev_free_block;
  struct meta_data * next_free_block;
} meta_data_t;
/*
meta_data_t * free_list_head = NULL;
meta_data_t * free_list_tail = NULL;
unsigned long segment_size = 0;
unsigned long segment_free_space_size = 0;
*/
typedef meta_data_t* (*add_func_t) (size_t);

// multi_thread malloc & free functions
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

// functions for performance study
unsigned long get_data_segment_size(); // in bytes
unsigned long get_data_segment_free_space_size(); // in bytes

// functions used for implementing malloc
void * f_malloc(size_t size, meta_data_t** free_list_head, meta_data_t** free_list_tail, int has_lock);

// function that returns the adress of an appropriate block in free list
// if it exists, remove it from free list return its address; else, return NULL
void * try_existed_block(size_t size, meta_data_t** free_list_head, meta_data_t** free_list_tail);

// function that tries to find an appropriate block in free list for BF
// if it exists, return it's address; else, return NULL
meta_data_t* find_existed_block(size_t size, meta_data_t** free_list_head);

// function that use sbrk() to add a new block
void * add_new_block(size_t size, int has_lock);

// functions used for implementing free
void f_free(void * ptr, meta_data_t** free_list_head, meta_data_t** free_list_tail);

// if block's previous free block or next free block is adjacent to it,
// then merge then into one free block
void try_coalesce(meta_data_t * block, meta_data_t** free_list_tail);

// functions used for manipulating free list
// add a block to free list
void add_to_free_list(meta_data_t * block, meta_data_t** free_list_head, meta_data_t** free_list_tail); // need to set is_used to 0

// remove a block from free list
void remove_block(meta_data_t * block, meta_data_t** free_list_head, meta_data_t** free_list_tail); // need to set is_used to 1

// split an unused block into two, used the first one
void split_block(meta_data_t* block1, size_t size, meta_data_t** free_list_head, meta_data_t** free_list_tail);

// functions for testing
void print_sizeof_metadata();
//void print_blocks();
void print_free_list(meta_data_t** free_list_head);
void print_block(meta_data_t * ptr);
