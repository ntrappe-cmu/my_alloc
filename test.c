#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h> 
#include <stdlib.h>
#include <stdio.h> 
#include <assert.h>
#include "my_alloc_internal.h"

// Simple test runner macro
#define RUN_TEST(test) \
  do { \
    printf("Running %s...\n", #test); \
    test(); \
    printf("✅ %s passed\n\n", #test); \
  } while (0)

/* ------- HELPER FUNCTION TESTS ------- */

void test_round_size_class() {
  assert(get_size_class(1) == 16);
  assert(get_size_class(15) == 16);
  assert(get_size_class(16) == 16);

  assert(get_size_class(17) == 32);
  assert(get_size_class(31) == 32);

  assert(get_size_class(510) == 512);
  assert(get_size_class(4096) == 4096);

  assert(get_size_class(0) == 0);     // Bad input
  assert(get_size_class(5000) == 0);  // Bad input
}

void test_alloc_init() {
  my_alloc_init();

  // Iterate over the list of pools and check that no pools exist
  for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
    assert(pools[i] == NULL);
  }

  assert(pool_count == 0); // No pools used
}

void test_first_pool_created(int pools_idx) {
  // We are only valid pointer in pools
  for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
    if (i == pools_idx) {
      assert(pools[i] != NULL);
    } else {
      assert(pools[i] == NULL);
    }
  }

  // Only first bitmap slot is used.
  for (int i = 0; i < pools[pools_idx]->num_slots; i++) {
    if (i == 0) {
      assert(pools[pools_idx]->bitmap[i] == SLOT_USED);
    } else {
      assert(pools[pools_idx]->bitmap[i] == SLOT_FREE);
    }
  }
}

int is_pool_empty(struct pool * p) {
  for (int i = 0; i < (int)p->num_slots; i++) {
    if (p->bitmap[i] == SLOT_USED)
      return 0;
  }

  return 1;
} 

void test_malloc_free1() {
  // First malloc call -> create new pool
  void * slab1 = my_malloc(16);
  uintptr_t first_slot_addr = (uintptr_t)slab1;
  assert(pool_count == 1);
  test_first_pool_created(0); // Only first pool non-NULL
 
  // Another malloc call to same pool
  void * slab2 = my_malloc(16);
  uintptr_t sec_slot_addr = (uintptr_t)slab2;
  assert(pool_count == 1); // No need to create ANOTHER POOL yet
  assert(pools[0]->bitmap[0] == SLOT_USED);
  assert(pools[0]->bitmap[1] == SLOT_USED);
  assert(pools[0]->bitmap[2] == SLOT_FREE);

  // Memory returned is within the same range
  assert(sec_slot_addr > first_slot_addr);
  assert(sec_slot_addr > (uintptr_t)pools[0]->base_addr);
  assert(sec_slot_addr < ((uintptr_t)pools[0]->base_addr + POOL_SIZE));

  // Between pools, no guarantee about pool positioning
  // Kernel can pick whatever virtual addresses it wants
  // Only can check slots WITHIN a pool
  // Use uintptr_t for arithmetic, void * for storage/passing
  assert(sec_slot_addr - first_slot_addr == 16);          // Exactly 1 slot apart

  // Now free up memory (just 1 pool)
  my_free(slab1);
  my_free(slab2);
  slab1 = NULL; // Good engineering practice
  slab2 = NULL;
  assert(pool_count == 1);  // This is a check of ALLOCATION, not active
  assert(pools[0] != NULL);
  assert(is_pool_empty(pools[0]) == 1); // True?
}

void test_malloc2() {
  void * slab0 = my_malloc(16);

  void * slab1 = my_malloc(4096);
  assert(pool_count == 2); // Bc of prev call
  assert(pools[0] != NULL);
  assert(pools[7] == NULL);
  assert(pools[8] != NULL);

  size_t num_slots = pools[8]->num_slots;
  assert(num_slots == 16);

  // Fill up pool for biggest size memory slots
  for (int i = 1; i < num_slots; i++) {
    void * slab_big = my_malloc(4096);
    assert(pools[8]->bitmap[i] == SLOT_USED);
  }
  assert(pool_count == 2); // No overflow yet

  // Now trigger overflow --> create new pool
  void * slab17 = my_malloc(4096);
  assert(pool_count == 3); 
  assert(pools[8]->next != NULL); // Pool 0 and 1 should be linked

  // Check that we have separate pools (enough memory btw)
  uintptr_t pool1 = (uintptr_t)slab1;
  uintptr_t pool2 = (uintptr_t)slab17;
  assert(pool2 - pool1 > 65536);

  // We have a memory leak (didn't free)
}

void test_free1() {
  void * slab1 = my_malloc(16);
  void * slab2 = my_malloc(32);
  void * slab3 = my_malloc(64);
  void * slab4 = my_malloc(128);
  // assert(pool_count == 5);
  printf("pool count: %d\n", pool_count);
  assert(pools[0] != NULL);
  assert(pools[1] != NULL);
  assert(pools[2] != NULL);
  assert(pools[3] != NULL);
  // assert(pools[4] == NULL);
  // assert(pools[8] == NULL);
}

void test_sanity() {
  my_alloc_init();
  assert(pool_count == 0);
  assert(pools[0] == NULL);

  void * ptr1 = my_malloc(16);
  // assert(pool_count == 1);
  // assert(pools[0] != NULL);

  // my_free(ptr1);
  // assert(pool_count == 1);
}

/* ------- MAIN TEST RUNNER ------- */
int main() {
  printf("Starting tests...\n\n");
  
  // RUN_TEST(test_round_size_class);
  // RUN_TEST(test_alloc_init);
  // RUN_TEST(test_malloc_free1);
  // RUN_TEST(test_malloc_free2);
  // RUN_TEST(test_free1);

  RUN_TEST(test_sanity);

  printf("===== ALL TESTS PASSED! =====\n");
  return 0;
}