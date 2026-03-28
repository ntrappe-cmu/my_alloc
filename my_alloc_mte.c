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
  p->bitmap = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // Now we differ from standard create_pool
  p->base_addr = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE | PROT_MTE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  p->next = NULL;

  if (p->bitmap == MAP_FAILED || p->base_addr == MAP_FAILED) abort();
  return p;
}

void *my_malloc_mte(size_t size) {
  size_t slot_size = get_size_class(size);
  if (!slot_size) return NULL;

  int idx = size_class_to_index(slot_size);
  struct pool *p = pools[idx];
  size_t slot_idx;

  while (p && find_first_free_slot(p->bitmap, p->num_slots, &slot_idx) == EXIT_FAILURE) {
    p = p->next;
  }

  if (!p) {
    p = create_pool_mte(slot_size);
    p->next = pools[idx];
    pools[idx] = p;
    slot_idx = 0;
  } else {
    find_first_free_slot(p->bitmap, p->num_slots, &slot_idx);
  }

  p->bitmap[slot_idx] = SLOT_USED;
  void *untagged_addr = (uint8_t*)p->base_addr + (slot_idx * p->slot_size);

  // Now we differ from standard malloc
  // Step 1: Create a RANDOM tag in the upper bits of the pointer
  //    For now, we don't restrict what gets chosen
  void *tagged_ptr = __arm_mte_create_random_tag(untagged_addr, 0);

  // Step 2: Paint the memory granules with the tag
  stamp_tag(tagged_ptr, p->slot_size);

  memset(tagged_ptr, 0, p->slot_size);
  return tagged_ptr;
}