#ifndef MY_ALLOC_H
#define MY_ALLOC_H

#include <stddef.h>

/**
 * Initializes the memory allocator. Must be called before any calls to malloc.
 */
void my_alloc_init(void);

/**
 * Cleans up the memory allocator after use.
 */
void my_alloc_destroy(void);

/**
 * my_malloc - Allocate a block of memory of at least @size bytes.
 * 
 * @size: Number of bytes requested. Will be rounded up to the nearest size
 *        class (16, 32, 64, ..., 4096). Returns NULL if @size is 0 or exceeds
 *        the maximum size class.
 * 
 * Returns: Pointer to the allocated block or NULL on failure.
 *          The returned memory is zeroed.
 */
void *my_malloc(size_t size);
void *my_malloc_mte(size_t size);

/**
 * my_free - Deallocate a block of memory specified by pointer @ptr.
 * 
 * @ptr: Pointer to memory block to be deallocated.
 */
void my_free(void *ptr);

#endif
