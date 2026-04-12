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
    printf("=== Use-After-Free ===\n");
    // 1. Allocate a 16-byte slot and cast it
    int *ptr = (int *)ALLOC(16);
    // 2. Write data to it
    *ptr = 42;
    // 3. Free it
    FREE(ptr);
    // 4. Read through stale pointer
    printf("Value %d\n", *ptr);
}

void test_heap_buffer_overflow() {
    printf("=== Heap Buffer Overflow ===\n");
    // 1. Allocate a 16-byte slot and cast it
    uint8_t *ptr = (uint8_t *)ALLOC(16);
    // 2. Write past the end (byte 16, 17, etc)
    for (int i = 0; i <= 16; i++) {
        ptr[i] = i + 1;
    }
    FREE(ptr);
}

void test_double_free() {
    printf("=== Double Free ===\n");
    // 1. Allocate a slot
    void * ptr = ALLOC(128);
    // 2. Free it
    FREE(ptr);
    // 3. Free again
    FREE(ptr);
}

void test_uninitialized_read() {
    printf("=== Uninitialized Read ===\n");
    // 1. Allocate slot
    int *ptr = (int *)ALLOC(2048);
    // 2. Read before writing to it
    printf("Value %d\n", *ptr);
    // 3. Verify contents are zero
    if (*ptr == 0) {
        printf("Contents have been zeroed\n");
    } else {
        printf("Contents were not zeroed\n");
    }
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