#include "my_alloc_internal.h"

/* ------------------------- Main Data Structures ------------------------- */

int pool_count = 0;                     // Num pools created, NOT active
struct pool *pools[NUM_SIZE_CLASSES];   // Metadata lives in BSS
struct pool pool_storage[MAX_POOLS];    // Metadata lives in BSS
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
    return -1;
}

int find_first_free_slot(uint8_t *bitmap, size_t num_slots, size_t *slot) {
    if (!bitmap) return -1;

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

    uint8_t *addr = (uint8_t *)ptr;

    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        struct pool *p = pools[i];
        while (p) {
            uint8_t *start = (uint8_t *)p->base_addr;
            uint8_t *end = start + POOL_SIZE;
            if (addr >= start && addr < end) return p;
            p = p->next;
        }
    }
    return NULL;
}

struct pool* create_new_pool(size_t slot_size) {
    if (pool_count >= MAX_POOLS) return NULL;

    struct pool *p = &pool_storage[pool_count++];

    p->slot_size = slot_size;
    p->num_slots = POOL_SIZE / slot_size;
    p->next = NULL;
  
    // Allocate PAGE for bitmap where each byte maps to 1 slot
    p->bitmap = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // Allocate entire pool of memory for each base address
    p->base_addr = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p->bitmap == MAP_FAILED || p->base_addr == MAP_FAILED) {
        fprintf(stderr, "@create_new_pool: Failed allocate memory for pool\n");
        return NULL;
    }
    return p;
}

/* ------------------------ Main External Functions ------------------------ */

void my_alloc_init() {
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        pools[i] = NULL;
    }
    pool_count = 0;
}

void my_alloc_destroy() {
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        struct pool *p = pools[i];

        while (p) {
            munmap(p->base_addr, POOL_SIZE);
            munmap(p->bitmap, PAGE_SIZE);
            p = p->next;
        }
        pools[i] = NULL;
    }
    pool_count = 0;
}

void *my_malloc(size_t size) {
    /* Step 1: Round up to size class */
    size_t slot_size = get_size_class(size);
    if (slot_size == 0) return NULL;

    /* Step 2: Map size class to index */
    int idx = size_class_to_index(slot_size);
    if (idx == -1) return NULL;

    struct pool *p = pools[idx];
    size_t slot_idx;

    /* Step 3: Search existing pools for free slot */
    while (p && find_first_free_slot(p->bitmap, p->num_slots, &slot_idx) != EXIT_SUCCESS) {
        p = p->next;
    }

    /* Step 4: If none found, create a new pool and insert at head */
    if (!p) {
        p = create_new_pool(slot_size);
        if (!p) return NULL;
        p->next = pools[idx];
        pools[idx] = p;
        slot_idx = 0;
    }

    /* Step 5: Mark slot used, zero memory, return pointer */
    p->bitmap[slot_idx] = SLOT_USED;
    void *res = (uint8_t*)p->base_addr + (slot_idx * p->slot_size);
    memset(res, 0, p->slot_size);
    return res;
}

void my_free(void *ptr) {
    if (!ptr) return;

    struct pool *p = lookup_pool(ptr);
    if (!p) {
        fprintf(stderr, "Failed to find a pool to which this pointer belongs.\n");
        return;
    }

    size_t offset = (uintptr_t)ptr - (uintptr_t)p->base_addr;
    if (offset % p->slot_size != 0) {
        fprintf(stderr, "Pointer is not aligned.\n");
        return;
    }

    size_t idx = offset / p->slot_size;
    if (p->bitmap[idx] == SLOT_FREE) {
        fprintf(stderr, "Double free detected!\n");
        return;
    }
    p->bitmap[idx] = SLOT_FREE;
    memset(ptr, 0, p->slot_size);
}
