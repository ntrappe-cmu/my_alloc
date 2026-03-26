#ifndef MY_ALLOC_INTERNAL_H
#define MY_ALLOC_INTERNAL_H

#include <sys/mman.h>             // mmap, munmap
#include <stddef.h>               // size_t, NULL
#include <stdint.h>               // uint8_t, uintptr_t
#include <string.h>               // memset (for zeroing)
#include <stdlib.h>               // EXIT_SUCCESS, EXIT_FAILURE, abort()
#include <stdio.h>                // fprintf, stderr
#include <math.h>
#include "my_alloc.h"

/* ------------------------------- Constants ------------------------------- */

#define PAGE_SIZE         4096    
#define POOL_SIZE         65536   // 65KB for whole pool
#define MAX_SIZE_CLASS    4096    // Largest size class offered
#define MIN_SIZE_CLASS    16      // Smallest size class offered (64bit arch)
#define MAX_POOLS			    256			// Generous upper bound
#define NUM_SIZE_CLASSES  9       // Arbitrary for now
#define SLOT_USED         1       // Bitmap flag to indicate slot used
#define SLOT_FREE         0	      // Bitmap flag to indicate not used
#define MTE_GRANULE       6       // MTE tags at 16-byte granularity

/**
 * Pool of memory for a given size class.
 * 
 * @base_addr: Memory address for the start of the pool. Chosen by kernel.
 * @bitmap: Each byte tracks one slot (free/used).
 * @slot_size: Size in bytes of a slot. slot_size * num_slots = POOL_SIZE.
 * @num_slots: Number of slots in the pool.
 * @next: Pointer to next pool. Otherwise, NULL.
 */
struct pool {
	void *base_addr;    // Match convention from mmap
	uint8_t *bitmap;    // Per byte slot
	size_t slot_size;   // Match convention for mmap, etc.
	size_t num_slots;   // Match convention for mmap, etc.
	struct pool *next;  // Pointer to next pool if exists
};

/* ------------------------- External Declarations ------------------------- */
extern int pool_count;                              // Num pools CREATED
extern struct pool *pools[NUM_SIZE_CLASSES];        // 1 pool list / size class
extern struct pool pool_storage[MAX_POOLS];         // All mem for pools
extern const size_t size_classes[NUM_SIZE_CLASSES]; // Size classes supported

/**
 * get_size_class - Rounds up to nearest size class.
 * 
 * @requested_size: Bytes requested.
 * 
 * Returns: Size class from set of supported ones. 0 on failure.
 */
size_t get_size_class(size_t requested_size);

/**
 * size_class_to_index - Maps a size_class to index of size_classes[].
 * 
 * @size_class: Size class from set of supported ones.
 * 
 * Returns: Index into size class set. Otherwise, -1.
 */
int size_class_to_index(size_t size_class);

/**
 * find_first_free_slot - Looks for first free slot (marked FREE) in bitmap for
 *                        a given pool.
 * 
 * @bitmap: Pointer to bitmap. Each byte represents a slot status.
 * @num_slots: Number of slots for this pool.
 * @slot: Pass-thru for slot selection.
 * 
 * Returns: EXIT_SUCCESS if free slot found; otherwise, EXIT_FAILURE.
 */
int find_first_free_slot(uint8_t *bitmap, size_t num_slots, size_t *slot);

/**
 * lookup_pool - Walk all pools to find which pool this pointer belongs to
 * 
 * @ptr: Pointer for a slot in a pool
 * 
 * Returns: Pointer to pool it corresponds to; otherwise, NULL.
 */
struct pool * lookup_pool(void * ptr);

/**
 * find_free_pool - Finds pool of size class with a free slot. 
 * TODO: if we have a lot of pools in a size class, could blow the stack.
 * 
 * @root: Pointer to first pool of the list.
 * 
 * Returns: Pointer to pool with space; otherwie, NULL.
 */
struct pool * find_free_pool(struct pool * root);

/**
 * create_new_pool - Creates a pool for a given size class.
 * 
 * @slot_size: The size class.
 * 
 * Returns: Pointer to the new pool. NULL if failure occurred.
 */
struct pool * create_new_pool(size_t slot_size);

/* ------------------------ Memory-Specific Actions ------------------------ */

/**
 * Zeroes out memory for a given block.
 */
void zero_memory(void * base_address, size_t slot_size);

/* ------------------------ Debugging/Observability ------------------------ */

// void print_pools() {
// 	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
// 		struct pool * p = pools[i];
// 		printf("+---------- START %000d ----------+\n", (int)size_classes[i]);
// 		if (!p) {
// 			printf("| NULL \t\t\t\t |\n");
// 		} else { 
// 			printf("| Num Slots: %d \t\t|\n", (int)p->num_slots);
// 			printf("| Size: %d \t\t\t|\n", (int)p->slot_size);
// 			int pool_num = 0;
// 			while (p) {
// 				printf("| Pool %d: %p \t\t|\n", pool_num, p->base_addr);
// 				printf("| Bitmap: \t\t\t|\n");
// 				for (int i = 0; i < 16; i++) {
// 					printf("| %s ", p->bitmap[i] == SLOT_FREE ? "FREE" : "USED");
// 				}
// 				printf("|...|\n");
// 				p = p->next;
// 				pool_num++;
// 			}
// 		}
// 		printf("+----------- END %000d -----------+\n", (int)size_classes[i]);
// 	}
// }

#endif