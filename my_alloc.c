#include "my_alloc_internal.h"

/* ------------------------- Main Data Structures ------------------------- */

int pool_count = 0;		// Num pools created, NOT active	
struct pool *pools[NUM_SIZE_CLASSES]; 
struct pool pool_storage[MAX_POOLS];
const size_t size_classes[NUM_SIZE_CLASSES] = 
	{16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

/* ----------------------------- Converters ----------------------------- */

size_t get_size_class(size_t requested_size) {
	// Step 1: Check if request is greater than 0 and less than max size
	if (requested_size == 0 || requested_size > MAX_SIZE_CLASS)
		return 0;

  // Smallest size based on architecture
  if (requested_size <= MIN_SIZE_CLASS) return MIN_SIZE_CLASS;

	// Step 2: Find the nearest rounded up size 
  // This is just powers of 2 so we can use a bit trick
	requested_size--;
  requested_size |= requested_size >> 1;
  requested_size |= requested_size >> 2;
  requested_size |= requested_size >> 4;
  requested_size |= requested_size >> 8;
  requested_size |= requested_size >> 16;
  requested_size |= requested_size >> 32;
  requested_size++;

	return requested_size;
}

int size_class_to_index(size_t size_class) {
	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		if (size_classes[i] == size_class)
			return i;
	}
	return -1;
}

/* ----------------------- Searching/Creating Pools ----------------------- */

int find_first_free_slot(uint8_t *bitmap, size_t num_slots, size_t *slot) {
  if (!bitmap) {
		fprintf(stderr, "@find_first_free_slot: Didn't get a valid bitmap pointer");
		abort();
	}

	for (size_t i = 0; i < num_slots; i++) {
		if (bitmap[i] == SLOT_FREE) {
      *slot = i;
      return EXIT_SUCCESS;
    }
	}

	return EXIT_FAILURE; // No free slot found
}

struct pool * lookup_pool(void * ptr) {
  if (!ptr)
		return NULL;

	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		// Iterate over each size class list
		struct pool * root = pools[i];

		while (root) {
			// For each pool in the list, check if pointer belongs
			if (ptr >= root->base_addr && 
					ptr < (void *)((uint8_t *)root->base_addr + POOL_SIZE)) {
				return root;
			}
			root = root->next;
		}
	}

	return NULL;
}

struct pool * create_new_pool(size_t slot_size) {
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

  return p;
}

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

/* ------------------------ Memory-Specific Actions ------------------------ */

/**
 * Zeroes out memory for a given block.
 */
void zero_memory(void * base_address, size_t slot_size) {
	memset(base_address, 0, slot_size);
}


/* ------------------------ Main External Functions ------------------------ */

void my_alloc_init() {
	// Step 1: Zero out the pools array
	// Each entry is a pointer to the first pool of that size class
	// They start as NULL because that means no pool has been allocated (lazy)
	for(int i = 0; i < NUM_SIZE_CLASSES; i++) {
		pools[i] = NULL;
	}

	// Step 2: Reset the pool storage counter
	pool_count = 0;
}


void * my_malloc(size_t size) {
	// Step 1: Pick the right size class by rounding up to the nearest size	
	size_t slot_size = get_size_class(size);
	if (slot_size == 0)	// Invalid requested size given
		return NULL;

	// Step 2: Find a pool with a free slot
	int idx = size_class_to_index(slot_size);
	struct pool * p = find_free_pool(pools[idx]);
	if (p)
		printf("Found pool %lx with free slot\n", (uintptr_t)p->base_addr);

	// Step 3: If no pool has space, create a new one
	if (!p) {
		p = create_new_pool(slot_size);
		struct pool * root = pools[idx];
		printf("Had to create pool %lx with free slot\n", (uintptr_t)p->base_addr);

		if (!root) {
			// No root pool yet, create the first one
			pools[idx] = p;
		} else {
			// There is a root so keep iterating until we find a free next ptr
			while (root->next) {
				root = root->next;
			}
			root->next = p;
		}
	}

	// Step 4: Find free slot within pool
	size_t slot_idx;
	if (find_first_free_slot(p->bitmap, p->num_slots, &slot_idx) == EXIT_FAILURE) {
		return NULL;
	}

	// Step 5: Mark as used
	p->bitmap[slot_idx] = SLOT_USED;

	// Step 6: Complete address of slot
	void * slot_address = (uint8_t *)p->base_addr + (slot_idx * p->slot_size);

	// Step 7: Zero out memory before handing it over
	zero_memory(slot_address, p->slot_size);

  return slot_address;
}


void my_free(void *ptr) {
	// Step 1: Figure out which pool this pointer belongs to
	// Start with just walking all the pools and chcking if pointer falls within
	struct pool * p = lookup_pool(ptr);
	if (!p) {
		fprintf(stderr, "Failed to find a pool to which this pointer belongs.\n");
		abort();
	}

	// Step 2: Validate the pointer (start of slot & used)
	size_t offset = ptr - p->base_addr;
  if (offset % p->slot_size != 0) {
		fprintf(stderr, "Pointer is not aligned.\n");
		abort();
	}

	int slot_idx = offset / p->slot_size;

	// Step 3: Mark the slot as free
	if (p->bitmap[slot_idx] == SLOT_FREE) {
		fprintf(stderr, "Double free detected!\n");
		abort();
	}

	p->bitmap[slot_idx] = SLOT_FREE;

	// Step 4: Zero the slot
	// Prevent use-after-free from reading stale data
	zero_memory(ptr, p->slot_size);
}