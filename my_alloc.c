#include "my_alloc_internal.h"

/* ------------------------- Main Data Structures ------------------------- */

int pool_count = 0;		// Num pools created, NOT active	
struct pool *pools[NUM_SIZE_CLASSES]; 
struct pool pool_storage[MAX_POOLS];
const size_t size_classes[NUM_SIZE_CLASSES] = 
	{16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

/* ---------------------------- Shared Helpers ----------------------------- */

size_t get_size_class(size_t requested_size) {
	// Step 1: Check if request is within bounds
	if (requested_size == 0 || requested_size > MAX_SIZE_CLASS) return 0;
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
		if (size_classes[i] == size_class) return i;
	}
	return EXIT_FAILURE;
}

int find_first_free_slot(uint8_t *bitmap, size_t num_slots, size_t *slot) {
  if (!bitmap) return EXIT_FAILURE;

	for (size_t i = 0; i < num_slots; i++) {
		if (bitmap[i] == SLOT_FREE) {
      *slot = i;
      return EXIT_SUCCESS;
    }
	}
	return EXIT_FAILURE;
}

/* ----------------------- Searching/Creating Pools ----------------------- */

struct pool* lookup_pool(void *ptr) {
  if (!ptr) return NULL;

  // Step 1: Convert to uintptr_t to perform bitwise masking safely
  uintptr_t raw_addr = (uintptr_t)ptr;
  uintptr_t untagged_addr = raw_addr & ~((uintptr_t)0xFF << 56);

  // Step 2: Cast to uint8_t* for range comparisons
  uint8_t *addr = (uint8_t *)untagged_addr;

	for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
		struct pool *p = pools[i]; // Iterate over each size class list
		while (p) {
      uint8_t *start = (uint8_t *)p->base_addr;
      uint8_t *end = start + POOL_SIZE;
			if (addr >= start && addr < end) {
        // Check if address belongs to pool
        return p;
      }
			p = p->next;
		}
	}
	return NULL;
}

struct pool * create_new_pool(size_t slot_size) {
  // Step 1: Check if we can create any more pools
  if (pool_count >= MAX_POOLS) return NULL;

  // Step 2: Use statically allocated memory for our pool
  struct pool *p = &pool_storage[pool_count++];

  // Step 3: Initialize and fill out fields
  p->slot_size = slot_size;
  p->num_slots = POOL_SIZE / slot_size;
  
  // Ensure bitmap is at least one page
  p->bitmap = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  p->base_addr = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  p->next = NULL;

  if (p->bitmap == MAP_FAILED || p->base_addr == MAP_FAILED) {
		fprintf(stderr, "@create_new_pool_mte: Failed to use mmap to allocate memory\n");
		abort();
	}
  return p;
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

void *my_malloc(size_t size) {
	// Step 1: Pick the right size class by rounding up to the nearest size	
	size_t slot_size = get_size_class(size);
	if (!slot_size)	return NULL;

	// Step 2: Find a pool with a free slot
	int idx = size_class_to_index(slot_size);
  if (idx == EXIT_FAILURE) return NULL;
	struct pool *p = pools[idx];
  size_t slot_idx;

  // Step 3: Find pool with space
  while (p && find_first_free_slot(p->bitmap, p->num_slots, &slot_idx) == EXIT_FAILURE) {
    p = p->next;
  }

	// Step 3: If no pool has space, create a new one or fetch its free slot
	if (!p) {
		p = create_new_pool(slot_size);
    p->next = pools[idx];
    pools[idx] = p;
    slot_idx = 0;
		printf("Had to create pool %lx with free slot\n", (uintptr_t)p->base_addr);
  } else {
    find_first_free_slot(p->bitmap, p->num_slots, &slot_idx);
  }

	// Step 5: Mark as used
	p->bitmap[slot_idx] = SLOT_USED;

	// Step 6: Complete address of slot
	void *res = (uint8_t*)p->base_addr + (slot_idx * p->slot_size);

	// Step 7: Zero out memory before handing it over
	memset(res, 0, p->slot_size);
  return res;
}


void my_free(void *ptr) {
  if (!ptr) return;
	// Step 1: Figure out which pool this pointer belongs to
	// Start with just walking all the pools and chcking if pointer falls within
	struct pool *p = lookup_pool(ptr);
	if (!p) {
		fprintf(stderr, "Failed to find a pool to which this pointer belongs.\n");
		abort();
	}

	// Step 2: Validate the pointer (start of slot & used)
	size_t offset = (uintptr_t)UNTAG_PTR(ptr) - (uintptr_t)p->base_addr;
  if (offset % p->slot_size != 0) {
		fprintf(stderr, "Pointer is not aligned.\n");
		abort();
	}

	size_t idx = offset / p->slot_size;

	// Step 3: Mark the slot as free
	if (p->bitmap[idx] == SLOT_FREE) {
		fprintf(stderr, "Double free detected!\n");
		abort();
	}
	p->bitmap[idx] = SLOT_FREE;

	// Step 4: Zero the slot
	// Prevent use-after-free from reading stale data
	memset(UNTAG_PTR(ptr), 0, p->slot_size);
}