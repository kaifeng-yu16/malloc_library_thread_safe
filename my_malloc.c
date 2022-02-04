#include "my_malloc.h"

meta_data_t * free_list_head_lock = NULL;
meta_data_t * free_list_tail_lock = NULL;
__thread meta_data_t * free_list_head_nolock = NULL;
__thread meta_data_t * free_list_tail_nolock = NULL;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

// multi_thread malloc function: lock version
void *ts_malloc_lock(size_t size) {
    pthread_mutex_lock(&mut);
    void * ret = f_malloc(size, &free_list_head_lock, &free_list_tail_lock, 1);
    pthread_mutex_unlock(&mut);
    return ret;
}

// multi_thread free function: lock version
void ts_free_lock(void *ptr) {
    pthread_mutex_lock(&mut);
    f_free(ptr, &free_list_head_lock, &free_list_tail_lock);
    pthread_mutex_unlock(&mut);
}

// multi_thread malloc function: nolock version
void *ts_malloc_nolock(size_t size) {
    return f_malloc(size, &free_list_head_nolock, &free_list_tail_nolock, 0);
}

// multi_thread free function: nolock version
void ts_free_nolock(void *ptr) {
    f_free(ptr, &free_list_head_nolock, &free_list_tail_nolock);
}

// malloc funtion 
// size: input, the size that need to be malloc
// f: a function used to find appropriate free block
void * f_malloc(size_t size, meta_data_t** free_list_head, meta_data_t** free_list_tail, int has_lock) {
  if (size == 0) {
    return NULL;
  }
  void * addr = NULL;
  if ((addr = try_existed_block(size, free_list_head, free_list_tail)) == NULL) {
    // require a new block
    void * ptr = add_new_block(size, has_lock);
    if (ptr == (void *) -1) {
      return NULL;
    } else {
      return ptr + sizeof(meta_data_t);
    }
  }
  assert(((meta_data_t *)addr)->is_used == 1);
  return addr + sizeof(meta_data_t);
}

// function that returns the adress of an appropriate block in free list
// if it exists, remove it from free list return it's address; else, return NULL
// size: input, the size that need to be malloc
// f: a function used to find appropriate free block
// return: if there is an appropriate block, return its address; else, return NULL
void * try_existed_block(size_t size, meta_data_t** free_list_head, meta_data_t** free_list_tail) {
  meta_data_t * ptr = find_existed_block(size, free_list_head);
  // no available free block
  if (ptr == NULL) {
    return NULL;
  }
  assert(ptr->is_used == 0);
  // need to seperate into two blocks
  if (ptr->size > size + sizeof(meta_data_t)) {
    split_block(ptr, size, free_list_head, free_list_tail);
  } else {
    remove_block(ptr, free_list_head, free_list_tail);
  }
  return ptr;
}

// function that tries to find an appropriate block in free list for BF
// if it exists, return it's address; else, return NULL
// size: input, the size that need to be malloc
// return: if there is an appropriate block, return its address; else, return NULL
meta_data_t * find_existed_block(size_t size, meta_data_t** free_list_head) {
  meta_data_t * ptr = *free_list_head;
  size_t min = SIZE_MAX;
  meta_data_t * min_ptr = NULL;
  while (ptr != NULL) {
    assert(ptr->is_used == 0);
    if(ptr->size == size) {
      return ptr;
    } else if (ptr->size > size && ptr->size < min) {
      min = ptr->size;
      min_ptr = ptr;
    }
    ptr = ptr->next_free_block;
  }
  return min_ptr;
}

// function that use sbrk() to add a new block
// size: input, the size that need to be malloc
// return: return the pointer returned by sbrk()
void * add_new_block(size_t size, int has_lock) {
  if (has_lock == 0) {
    pthread_mutex_lock(&mut);
  }
  void * ptr = sbrk(size + sizeof(meta_data_t));
  if (has_lock == 0) {
    pthread_mutex_unlock(&mut);
  }
  if (ptr == (void * )-1) {
    return ptr;
  }
  meta_data_t * block_meta = (meta_data_t *) ptr;
  block_meta->size = size;
  block_meta->is_used = 1;
  block_meta->prev_free_block = NULL;
  block_meta->next_free_block = NULL;
  
  return ptr;
}

// free funtion that can be used by both FF and BF
// ptr: input, the ptr that needs to be freed
void f_free(void * ptr, meta_data_t** free_list_head, meta_data_t** free_list_tail) {
  if (ptr == NULL) {
    return;
  }
  assert ((unsigned long)ptr >= sizeof(meta_data_t));
  meta_data_t * block = (meta_data_t *) (ptr - sizeof(meta_data_t));
  assert(block->is_used == 1);
  add_to_free_list(block, free_list_head, free_list_tail); 
  assert (block->is_used == 0);
  try_coalesce(block, free_list_tail);
}

// if block's previous free block or next free block is adjacent to it,
// then merge then into one free block
// block: input, the block needs to be merged
void try_coalesce(meta_data_t * block, meta_data_t** free_list_tail) {
  assert(block->is_used == 0);
  if (block->next_free_block != NULL &&
    (void *)(block) + sizeof(meta_data_t) + block->size == block->next_free_block) {
    assert(block->next_free_block->is_used == 0);
    block->size += (block->next_free_block->size + sizeof(meta_data_t));
    if (block->next_free_block->next_free_block != NULL) {
      block->next_free_block->next_free_block->prev_free_block = block;
    } else {
      *free_list_tail = block;
    }
    block->next_free_block = block->next_free_block->next_free_block; 
  }
  if(block->prev_free_block != NULL &&
    (void *)(block->prev_free_block) + sizeof(meta_data_t) + block->prev_free_block->size == block) {
    assert(block->prev_free_block->is_used == 0);
    block->prev_free_block->size += (block->size + sizeof(meta_data_t));
    if(block->next_free_block != NULL) {
      block->next_free_block->prev_free_block = block->prev_free_block;
    } else {
      *free_list_tail = block->prev_free_block;
    }
    block->prev_free_block->next_free_block = block->next_free_block;
  }
}

// add a block to free list
// block: input, the ptr of the block that needs to be add to free list
void add_to_free_list(meta_data_t * block, meta_data_t** free_list_head, meta_data_t** free_list_tail) {
  assert(block->is_used == 1);
  assert(block->prev_free_block == NULL && block->next_free_block == NULL);
  block->is_used = 0;
  meta_data_t ** ptr = free_list_head;
  while ((*ptr) != NULL) {
    if (*ptr > block) {
      break;
    }
    ptr = &((*ptr)->next_free_block);
  }
  block->next_free_block = *ptr;
  if ((*ptr) != NULL) {
    block->prev_free_block = (*ptr)->prev_free_block;
    (*ptr)->prev_free_block = block;
  } else {
    block->prev_free_block = *free_list_tail;
    *free_list_tail = block;
  }
  (*ptr) = block;
}

// remove a block from free list
// block: input, the ptr of the block that needs to be removed from free list
void remove_block(meta_data_t * block, meta_data_t** free_list_head, meta_data_t** free_list_tail) {
  assert(block->is_used == 0);
  block->is_used = 1;
  if (block->prev_free_block == NULL) {
    *free_list_head = block->next_free_block;
  } else {
    block->prev_free_block->next_free_block = block->next_free_block;
  }
  if (block->next_free_block == NULL) {
    *free_list_tail = block->prev_free_block;
  } else {
    block->next_free_block->prev_free_block = block->prev_free_block;
  }
  block->prev_free_block = NULL;
  block->next_free_block = NULL;
}

// split an unused block into two, use the first one
// block1: input, the block that need to be split
// size: the size of the first block
void split_block(meta_data_t * block1, size_t size, meta_data_t** free_list_head, meta_data_t** free_list_tail) {
  // important assertion
  assert(block1->is_used == 0);
  assert(block1->size - size > sizeof(meta_data_t));
  meta_data_t * block2 = (meta_data_t *)((void *)block1 + size + sizeof(meta_data_t));
  block2->prev_free_block = block1->prev_free_block;
  block2->next_free_block = block1->next_free_block;
  if (block2->next_free_block != NULL) {
    block2->next_free_block->prev_free_block = block2;
  } else {
    *free_list_tail = block2;
  }
  if (block2->prev_free_block != NULL) {
    block2->prev_free_block->next_free_block = block2;
  } else {
    *free_list_head = block2;
  }
  block1->is_used = 1;
  block2->is_used = 0;
  block2->size = block1->size - sizeof(meta_data_t) - size;
  block1->size = size;
  block1->prev_free_block = NULL;
  block1->next_free_block = NULL;
}

// print free list
void print_free_list(meta_data_t** free_list_head) {
  printf("***Free List Data***\n");
  int cnt = 0;
  meta_data_t * ptr = *free_list_head;
  while (ptr != NULL) {
    printf("free block %d [%lu]: is_used[%u], size[%lu], prev_free[%lu], next_free[%lu] \n", 
        cnt, (unsigned long) ptr, (unsigned int)ptr->is_used, ptr->size, (unsigned long)ptr->prev_free_block,
        (unsigned long)ptr->next_free_block);
    ptr = ptr->next_free_block;
    ++cnt;
  }
  printf("head[%lu], tail[%lu] \n", (unsigned long)free_list_head_lock, (unsigned long)free_list_tail_lock);
  printf("Total free blocks: %d\n\n", cnt);
}

// print a certain block
void print_block(meta_data_t * ptr) {
  printf("***Block Data***\n");
  printf("free block[%lu]: is_used[%u], size[%lu], prev_free[%lu], next_free[%lu] \n", 
      (unsigned long)ptr, (unsigned int)ptr->is_used, ptr->size, (unsigned long)ptr->prev_free_block,
      (unsigned long)ptr->next_free_block);
  printf("head[%lu], tail[%lu] \n", (unsigned long)free_list_head_lock, (unsigned long)free_list_tail_lock);
}

// pinrt the size of meta_data_t
void print_sizeof_metadata() {
  printf("sizeof meta_data_t is %ld\n", sizeof(meta_data_t));
}
