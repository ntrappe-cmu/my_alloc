#ifndef MY_ALLOC_INTERNAL_H
#define MY_ALLOC_INTERNAL_H

#include <sys/mman.h>				// mmap, munmap
#include <stddef.h>					// size_t, NULL
#include <stdint.h>					// uint8_t, uintptr_t
#include <string.h>					// memset (for zeroing)
#include <stdlib.h>					// EXIT_SUCCESS, EXIT_FAILURE, abort()
#include <stdio.h>					// fprintf, stderr
#include <arm_acle.h>				// ARM random tag

#include "my_alloc.h"

/* ------------------------------- Constants ------------------------------- */

#define POOL_SIZE 			65536	// 65KB for whole pool
#define MAX_SIZE_CLASS		4096    // Largest size class offered
#define MIN_SIZE_CLASS		16      // Smallest size class offered (64bit arch)
#define MAX_POOLS			256		// Generous upper bound
#define PAGE_SIZE 			4096	// POOL_SIZE / MIN_SIZE_CLASS

#define NUM_SIZE_CLASSES 	9       // Arbitrary for now
#define SLOT_USED			1       // Bitmap flag to indicate slot used
#define SLOT_FREE			0		// Bitmap flag to indicate not used

// MTE Granule is physically 16 bytes.
#define MTE_GRANULE_SIZE 	16

// Helper to strip the top-byte tags for address comparisons
// #define UNTAG_PTR(ptr) (void*)((uintptr_t)(ptr) & ~((uintptr_t)0xFF << 56))

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

/* ---------------------------- Shared Globals ---------------------------- */
extern int pool_count;                              // Num pools CREATED
extern struct pool *pools[NUM_SIZE_CLASSES];        // 1 pool list / size class
extern struct pool pool_storage[MAX_POOLS];         // All mem for pools
extern const size_t size_classes[NUM_SIZE_CLASSES]; // Size classes supported

/* ------------------------ Shared Internal Helpers ----------------------- */

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
 * lookup_pool - Walk all pools to find which pool this pointer belongs to
 * 
 * @ptr: Pointer for a slot in a pool
 * 
 * Returns: Pointer to pool it corresponds to; otherwise, NULL.
 */
struct pool* lookup_pool(void *ptr);

/**
 * find_first_free_slot - Looks for first free slot (marked FREE) in bitmap for
 *                        a given pool.
 * 
 * @bitmap: Pointer to bitmap. Each byte represents a slot status.
 * @num_slots: Number of slots for this pool.
 * @slot: Pass-thru for slot selection.
 * 
 * Returns: EXIT_SUCCESS if free slot found; otherwise, -1.
 */
int find_first_free_slot(uint8_t *bitmap, size_t num_slots, size_t *slot);

/**
 * create_new_pool - Creates a pool for a given size class.
 * 
 * @slot_size: The size class.
 * 
 * Returns: Pointer to the new pool. NULL if failure occurred.
 */
struct pool* create_new_pool(size_t slot_size);


#endif