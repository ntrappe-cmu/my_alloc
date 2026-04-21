#include "my_alloc_internal.h"

#ifndef PROT_MTE
#define PROT_MTE 0x20
#endif

/* ========================= MTE Helper Functions ========================= */

static void stamp_tag(void *tagged_ptr, size_t size) {
    // Iterate over slot and "color" each memory granule
    for (size_t i = 0; i < size; i += MTE_GRANULE_SIZE) {
        __arm_mte_set_tag((uint8_t *)tagged_ptr + i);
    }
}


static struct pool* create_pool_mte(size_t slot_size) {
    if (pool_count >= MAX_POOLS) return NULL;

    struct pool *p = &pool_storage[pool_count++];

    p->slot_size = slot_size;
    p->num_slots = POOL_SIZE / slot_size;
    p->next = NULL;

    // Almost same but also add MTE tagging support
    // @MTE
    p->bitmap = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    p->base_addr = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE | PROT_MTE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    

    if (p->bitmap == MAP_FAILED || p->base_addr == MAP_FAILED) {
        fprintf(stderr, "@create_new_pool: Failed allocate memory for pool\n");
        return NULL;
    }
    return p;
}

void *my_malloc_mte(size_t size) {
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
        p = create_pool_mte(slot_size);
        if (!p) return NULL;
        p->next = pools[idx];
        pools[idx] = p;
        slot_idx = 0;
    }

    /* Step 5: Mark slot used, zero memory, return pointer */
    p->bitmap[slot_idx] = SLOT_USED;

    /* Step 6: Create a random tag in the upper bits of the pointer (any are ok) */
    // @MTE
    void *untagged_addr = (uint8_t*)p->base_addr + (slot_idx * p->slot_size);
    void *tagged_ptr = __arm_mte_create_random_tag(untagged_addr, 0);

    /* Step 7: Paint memory granules with tag */
    stamp_tag(tagged_ptr, p->slot_size);

    memset(tagged_ptr, 0, p->slot_size);
    return tagged_ptr;
}

void my_free_mte(void *ptr) {
    if (!ptr) return;

    // Strip upper bits before we do a lookup
    void *untagged_ptr = UNTAG_PTR(ptr);
    struct pool *p = lookup_pool(untagged_ptr);
    if (!p) {
        fprintf(stderr, "Failed to find a pool to which this pointer belongs.\n");
        abort();
    }

    size_t offset = (uintptr_t)untagged_ptr - (uintptr_t)p->base_addr;
    if (offset % p->slot_size != 0) {
        fprintf(stderr, "Pointer is not aligned.\n");
        abort();
    }

    size_t idx = offset / p->slot_size;
    if (p->bitmap[idx] == SLOT_FREE) {
        fprintf(stderr, "Double free detected!\n");
        abort();
    }

    p->bitmap[idx] = SLOT_FREE;
    memset(untagged_ptr, 0, p->slot_size);

    // After memset retag so stale pointers fault
    void *slot_addr = (uint8_t *)p->base_addr + (idx * p->slot_size);
    void *new_tag = __arm_mte_create_random_tag(slot_addr, 0);
    stamp_tag(new_tag, p->slot_size);
}