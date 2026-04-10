#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "my_alloc.h"

/* Pick allocator based on compile flag */
#ifdef MTE_ENABLED
  #define ALLOC(sz)   my_malloc_mte(sz)
  #define FREE(ptr)   my_free_mte(ptr)
#else
  #define ALLOC(sz)   my_malloc(sz)
  #define FREE(ptr)   my_free(ptr)
#endif

void test_use_after_free() {
    // 1. Allocate a 16-byte slot and cast it
    int *ptr = (int *)ALLOC(16);
    // 2. Write data to it
    *ptr = 42;
    // 3. Free it
    FREE(ptr);
    printf("Value %d\n", *ptr);
}

void test_heap_buffer_overflow() {

}

void test_double_free() {

}

void test_uninitialized_read() {
}

int main(int argc, char *argv[]) {
    my_alloc_init();

    if (argc < 2) {
        printf("Usage: %s <test_num>\n", argv[0]);
        printf("  1 = use-after-free\n");
        printf("  2 = heap buffer overflow\n");
        printf("  3 = double free\n");
        printf("  4 = uninitialized read\n");
        return 1;
    }

    switch (argv[1][0]) {
        case '1': test_use_after_free(); break;
        case '2': test_heap_buffer_overflow(); break;
        case '3': test_double_free(); break;
        case '4': test_uninitialized_read(); break;
        default:  printf("Unknown test\n"); break;
    }

    my_alloc_destroy();
    return 0;
}