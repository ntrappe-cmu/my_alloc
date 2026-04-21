#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "my_alloc_internal.h"

#define RUN_TEST(test) \
  do { \
    printf("Running %s...\n", #test); \
    test(); \
    printf("PASS: %s\n\n", #test); \
  } while (0)

static void test_mte_sanity(void) {
  my_alloc_init();

  void *ptr = my_malloc_mte(16);
  assert(ptr != NULL);

  /* Returned memory should be zeroed. */
  for (size_t i = 0; i < 16; i++) {
    assert(((uint8_t *)ptr)[i] == 0);
  }

  /* Pointer should be aligned to the requested size class. */
  assert(((uintptr_t)ptr % 16) == 0);

  my_free_mte(ptr);

  /* Basic second allocation check after a free. */
  void *ptr2 = my_malloc_mte(16);
  assert(ptr2 != NULL);
  my_free_mte(ptr2);
}

int main(void) {
  printf("Starting MTE tests...\n\n");
  RUN_TEST(test_mte_sanity);
  printf("===== ALL MTE TESTS PASSED! =====\n");
  return 0;
}