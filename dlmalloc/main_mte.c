#define USE_DL_PREFIX

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "malloc.h"

int main() {
    int rc = system("echo hello > /tmp/test.txt");
    printf("system rc = %d\n", rc);

    int i = 0;
    size_t num_bytes = 16;
    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr2 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr2);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr3 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr3);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    num_bytes = 4096;
    void *tiny_ptr4 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr4);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    num_bytes = 262144;
    void *tiny_ptr5 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr5);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr6 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr6);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr7 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr7);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================== BAD MEMORY TESTS ========================\n", ++i);
    dlfree(tiny_ptr);

    /* THE FOLLOWING ARE COMMENTED OUT BECAUASE THEY WILL CRASH (GOOD) */
    
    // printf("TEST: Freed tiny_ptr. Attempting stale read...\n");
    // volatile uint8_t uaf_val = ((volatile uint8_t *)tiny_ptr)[0];
    // printf("!! UAF read succeeded (BUG NOT CAUGHT): %d\n", uaf_val);

    // printf("TEST: Flip one tag bit...\n");
    // volatile uint8_t *bad = (uint8_t *)((uintptr_t)tiny_ptr2 ^ (1UL << 56));
    // (void)*bad; /* if SYNC+MTE works, this SIGSEGVs immediately with si_code == SEGV_MTESERR */
    // printf("!! Tag mod did not fault (BUG NOT CAUGHT): %p\n", bad);

    // printf("TEST: Overflow chunk to next chunk...\n");
    // ((char*)tiny_ptr3)[17] = 'A';
    // printf("!! Buffer overflow succeeded (BUG NOT CAUGHT)\n");


    dlfree(tiny_ptr2);
    dlfree(tiny_ptr3);
    dlfree(tiny_ptr4);
    dlfree(tiny_ptr5);
    dlfree(tiny_ptr6);
    dlfree(tiny_ptr7);

    return EXIT_SUCCESS;
}