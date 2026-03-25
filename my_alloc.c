#include "my_alloc.h"

#include <sys/mman.h>   // mmap, munmap
#include <stddef.h>     // size_t, NULL
#include <stdint.h>     // uint8_t, uintptr_t
#include <string.h>     // memset (for zeroing)
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, abort()
#include <stdio.h>      // fprintf, stderr
#include <assert.h>			// for testing TODO

#define PAGE_SIZE           4096
#define POOL_SIZE           65536
#define MAX_POOLS						256			// Generous upper bound
#define NUM_SIZE_CLASSES    9
#define MTE_GRANULE         16      // MTE tags at 16-byte granularity
#define SLOT_USED						1
#define SLOT_FREE						0	

/**
 * Pool object
 */
struct pool {
	void *base_addr;
	uint8_t *bitmap;
	size_t slot_size;
	size_t num_slots;
	struct pool *next;
};

// Size classes our allocator supports
size_t size_classes[NUM_SIZE_CLASSES] = 
	{16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

int pool_count = 0;															// Num pools allocated so far

// Stack memory for now, use mmap later
static struct pool *pools[NUM_SIZE_CLASSES];		// One pool list per size class
static struct pool pool_storage[MAX_POOLS];			// Static for now (all mem for pools) 

void my_alloc_init() {
	// Step 1: Zero out the pools array
	// Each entry is a pointer to the first pool of that size class
	// They start as NULL because that means no pool has been allocated (lazy)
	for(int i = 0; i < NUM_SIZE_CLASSES; i++) {
		pools[i] = NULL;
	}

	// Step 2: Rest the pool storage counter
	pool_count = 0;
}

/**
 * Helper function to map a size_class to index of size_classes[].
 */
int size_class_to_index(size_t size_class) {
	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		if (size_classes[i] == size_class)
			return i;
	}
	return -1;
}

/**
 * Rounds to the nearest size class for a requested size.
 * Returns 0 if invalid requested size given.
 */
size_t get_size_class(size_t requested_size) {
	// Step 1: Check if request is greater than 0 and less than max size
	size_t max_size = size_classes[NUM_SIZE_CLASSES - 1];
	if (requested_size == 0 || requested_size > max_size)
		return 0;

	// Step 2: Find the nearest rounded up size
	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		if (requested_size <= size_classes[i])
		  return size_classes[i];
	}

	return 0;
}

/**
 * Finds a pool of size class with a free slot. Otherwise, returns NULL.
 * If we have a lot of pools in a size class, could blow the stack. TODO.
 */
struct pool* find_free_pool(struct pool * root) {
	if (!root)
		return NULL;
	
	// Check if this pool has a free slot
	for (size_t i = 0; i < root->num_slots; i++) {
    if (root->bitmap[i] == SLOT_FREE)
			return root;
	}

	// Recurse on the next pool
	return find_free_pool(root->next);
}

struct pool* create_new_pool(size_t slot_size) {
	// Step 1: Check if we can create any more pools
	if (pool_count >= MAX_POOLS) {
		fprintf(stderr, "@create_new_pool: Out of pool storage.\n");
		abort();
	}

	// Step 2: Use statically allocated memory for our pool
	struct pool * p = &pool_storage[pool_count];
	pool_count += 1;

	// Step 3: Initialize and fill out fields
	p->slot_size = slot_size;
	p->num_slots = POOL_SIZE / slot_size;
	// Let kernel pick address, pages may be read, private
	// mmap the bitmap (wastes a page for small bitmaps)
	p->bitmap = mmap(NULL, p->num_slots, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// mmap the actual memory slot
	p->base_addr = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// Required: pool_storage not re-zeroed right now between init cycles
	p->next = NULL;

	if (p->bitmap == MAP_FAILED || p->base_addr == MAP_FAILED) {
		fprintf(stderr, "@create_new_pool: Failed to use mmap to allocate memory\n");
		abort();
	}

	printf("Allocated pool for size %d\n", (int)slot_size);
  return p;
}


int find_first_free_slot(uint8_t *bitmap, size_t num_slots) {
  if (!bitmap) {
		fprintf(stderr, "@find_first_free_slot: Didn't get a valid bitmap pointer");
		abort();
	}

	for (int i = 0; i < (int)num_slots; i++) {
		if (bitmap[i] == SLOT_FREE)
			return i;
	}

	return -1; // No free slot found
}

/**
 * Zeroes out memory for a given block.
 */
void zero_memory(void * base_address, size_t slot_size) {
	memset(base_address, 0, slot_size);
}

/**
 * External function.
 * Round @size up to nearest size class. Walk pools for that class (or lookup),
 * scanning each bitmap for a free slot. If no pool has space, allocate a new
 * bool via mmap. Mark the slow as used, zero it, and return 
 * base_addr + (slot_index * slot_size).
 */
void *my_malloc(size_t size) {
	// Step 1: Pick the right size class by rounding up to the nearest size	
	size_t slot_size = get_size_class(size);
	if (slot_size == 0)	// Invalid requested size given
		return NULL;

	// Step 2: Find a pool with a free slot
	int idx = size_class_to_index(slot_size);
	struct pool * p = find_free_pool(pools[idx]);

	// Step 3: If no pool has space, create a new one
	if (!p) {
		p = create_new_pool(slot_size);
		struct pool * root = pools[idx];
		if (root) {
			p->next = root;
		}
		pools[idx] = p;
	}

	// Step 4: Find free slot within pool
	int slot_idx = find_first_free_slot(p->bitmap, p->num_slots);
	if (slot_idx == -1) {
		return NULL;
	}
	printf("Got slot %d\n", slot_idx);

	// Step 5: Mark as used
	p->bitmap[slot_idx] = SLOT_USED;

	// Step 6: Complete address of slot
	void * slot_address = (uint8_t *)p->base_addr + (slot_idx * p->slot_size);

	// Step 7: Zero out memory before handing it over
	zero_memory(slot_address, p->slot_size);

  return NULL;
}

void my_free(void *ptr) {

}

void print_pools() {
	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		struct pool * p = pools[i];
		printf("+---------- START %000d ----------+\n", (int)size_classes[i]);
		if (!p) {
			printf("| NULL \t\t\t\t |\n");
		} else { 
			printf("| Base Addr: %x \t\t|\n", p->base_addr);
			printf("| Num Slots: %d \t\t|\n", (int)p->num_slots);
		}
		printf("+----------- END %000d -----------+\n", (int)size_classes[i]);
	}

	struct pool {
	void *base_addr;
	uint8_t *bitmap;
	size_t slot_size;
	size_t num_slots;
	struct pool *next;
};
}

int main() {
  printf("--- Initialization ---\n");
  my_alloc_init();
	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		assert(pools[i] == NULL && "Init failed to NULL out each pointer");
	}

	printf("--- Size Classes ---\n");
	assert(get_size_class(1) == 16 && "Didn't round up 1 -> 16");
	assert(get_size_class(15) == 16 && "Didn't round up 15 -> 16");
	assert(get_size_class(16) == 16 && "Didn't do 16 -> 16");
	assert(get_size_class(31) == 32 && "Didn't do 31 -> 32");
	assert(get_size_class(0) == 0 && "Reject");
	

	printf("--- Malloc ---\n");
	void * slab = my_malloc(16);
	void * slab2 = my_malloc(16);
	void * slab3 = my_malloc(64);
	print_pools();

	return 0;
}