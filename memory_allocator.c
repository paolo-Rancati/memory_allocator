#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE.     */
#define CHUNK_SIZE (1<<12)

struct free_block {
  size_t header;
  struct free_block *next;
};

static struct free_block **free_list = NULL;
static int alloc_check = 0;
static int calloc_request = 0;

/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
  if (size == 0) {
    return NULL;
  }

  
  if (alloc_check == 0) { //only allocate the free_list ONCE
    free_list = sbrk(104);
    for (int i = 0; i <= 13; i++) {
      free_list[i] = NULL; //initially there is nothing to point to
    }
    alloc_check++;
  }

  if (size > ((CHUNK_SIZE) - (sizeof(size_t)))) { //use bulk_allocator for requests > CHUNK_SIZE - sizeof(size_t)
    void *bulk;
    bulk = bulk_alloc(size + sizeof(size_t));
    *(size_t *)bulk = size + 1;
    bulk += sizeof(size_t);
    return bulk;
  }
  
  //If there are no free blocks to allocate requested size
  if (free_list[block_index(size)] == NULL) { //if no free blocks of requested size
    free_list[block_index(size)] = sbrk(CHUNK_SIZE);  //must request more memory from OS
    if (calloc_request == 1) {
      memset(free_list[block_index(size)], 0, (size + sizeof(size_t))); //initialize this chunk to 0's if a calloc call
      calloc_request = 0;
    }
  
    void *ptr = free_list[block_index(size)];
    
    void *use = ptr;//just want to keep ptr where it is 
    
    size_t block_size = 1 << (block_index(size)); //the size of the free block including Metadata
    
    void *go; //I'll actually move this pointer
    
    size_t a = CHUNK_SIZE/block_size; //how many block_size blocks in sbrk(CHUNK_SIZE)
    struct free_block *block;
    
    for (size_t i = 0; i < a; i++) {
      block = (struct free_block *)(use + i*block_size);
      go = block;
      go += block_size;
      
      if (i == a-1) { //if this is the last block in the chunk, its next ptr is NULL
    block->next = NULL;
      }else {
    block->next = (struct free_block *)(go);
      }
      
      block->header = block_size; //assign header
    }
    
    
    struct free_block *pointer;
    pointer = (struct free_block *)(ptr);
    //mark this block as allocated
    pointer->header++;

    //change free_list
    free_list[block_index(size)] = pointer->next;
    pointer->next = NULL;

    //set all pointers to NULL
    pointer = NULL;
    use = NULL;
    go = NULL;

    //move ptr by the size of the header
    ptr += sizeof(size_t);
    return ptr;
    
  } else {
    //If there is memory of requested size available to be allocated
    void *ptr = free_list[block_index(size)];
    struct free_block *Pointer;
    Pointer = (struct free_block *)ptr;
    Pointer->header++; //mark as allocated
    free_list[block_index(size)] = Pointer->next;
    Pointer->next = NULL;
    Pointer = NULL; //is this an error?
    ptr += sizeof(size_t);
    if (calloc_request == 1) { //if this malloc call is from calloc memset to 0
      memset(ptr, 0, (size + sizeof(size_t)));
      calloc_request = 0;
    }
    return ptr;
  }
    
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
  size_t calloc_size = (nmemb * size);
  if (calloc_size == 0) {
    return NULL;
  }
  if (calloc_size > (CHUNK_SIZE - sizeof(size_t))) {
    void *bulk;
    bulk = bulk_alloc(calloc_size + sizeof(size_t));
    memset(bulk, 0, calloc_size + sizeof(size_t)); //initialize this memory to 0
    *(size_t *)bulk = calloc_size + 1; 
    bulk += sizeof(size_t);
    return bulk;
  }else {
    calloc_request++;
    void *ptr = malloc(calloc_size);
    return ptr;
  }
}



/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
  if (ptr == NULL) {
    return;
  }
  size_t free_size;
  free_size = *(size_t *)(ptr - sizeof(size_t));
  free_size--;
  if ((free_size) > CHUNK_SIZE) {
    bulk_free((ptr - sizeof(size_t)), (free_size + sizeof(size_t)));
    return; 
  } else {
    struct free_block *freed_block;
    freed_block = (struct free_block *)(ptr - sizeof(size_t));
    freed_block->header = free_size;
    freed_block->next = free_list[block_index(free_size - sizeof(size_t))];//free block next points to what free_list[index] pointed to
    free_list[block_index(free_size - sizeof(size_t))] = freed_block; //free_list now points to this block (change head)
    return;
  }

}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return malloc(size);
  }

  if (size == 0 && ptr != NULL) {
    free(ptr);
  }
  
  size_t input_size;
  input_size = *(size_t *)(ptr - sizeof(size_t));
  input_size--;

  //if input_size is larger than requested size, just return the ptr
  //dont change header, this block requires bulk_free
  if ((input_size) > size) {
    return ptr;
  }
  
  void *new;
  //determine if new should be done using malloc or bulk_alloc
  if (size > (CHUNK_SIZE - sizeof(size_t))) {
      new = bulk_alloc(size + sizeof(size_t));
      //possible issues here
      memcpy(new, (ptr - sizeof(size_t)), ((input_size) + sizeof(size_t)));
      *(size_t *)new = size + 1;
      new += sizeof(size_t);
      if ((input_size) > CHUNK_SIZE) {
    bulk_free((ptr - sizeof(size_t)), (input_size + sizeof(size_t)));
      } else {
    free(ptr);
      }
      return new;
  }else {
    new = malloc(size);
    int get_power = block_index(input_size - 1);
    size_t max_avail = ((1 << get_power) - (sizeof(size_t)));
    memcpy(new, ptr, max_avail);
    free(ptr);
    return new;
  }
}
