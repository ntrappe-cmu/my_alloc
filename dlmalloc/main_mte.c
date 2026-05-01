#define USE_DL_PREFIX

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "malloc.h"

int main() {

    // printf("======================= START MTE CALLOC =======================\n");
    // int *ptr0 = (int *)dlcalloc(5, sizeof(int));
    // if (ptr0 == NULL) {
    //     printf("Allocation failed\n");
    //     return 0;
    // } else {
    //     printf("Allocation succeeded, not null\n");
    // }
    
    // printf("[");
    // for (int j = 0; j < 5; j++) {
    //     printf("%d|", ptr0[j]);
    // }
    // printf("]\n");
    // dlfree(ptr0);
    // printf("========================== END CALLOC ==========================\n");

    int i = 0;
    size_t num_bytes = 16;
    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE REALLOC 1 =======================\n");
    printf("Try to realloc 16 bytes of prev ptr=%p...\n", tiny_ptr);
    void *re_tiny_ptr = dlrealloc(tiny_ptr, 32);
    dlfree(re_tiny_ptr);
    printf("========================== END REALLOC 1 ==========================\n");

    printf("======================= START MTE REALLOC 2 =======================\n");
    void *shrink_ptr = dlmalloc(128);
    printf("Allocated 128 bytes at %p\n", shrink_ptr);
    ((char *)shrink_ptr)[0] = 'A';  // write something to verify data survives
    void *shrink_result = dlrealloc(shrink_ptr, 32);
    printf("Realloc'd down to 32 bytes at %p\n", shrink_result);
    printf("Data preserved: %c\n", ((char *)shrink_result)[0]);
    dlfree(shrink_result);
    printf("========================== END REALLOC 2 ==========================\n");


    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr2 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr2);
    printf("========================== END ALLOC %d ==========================\n", i);

    printf("======================= START MTE REALLOC 3 =======================\n");
    void *adj1 = dlmalloc(32);
    void *adj2 = dlmalloc(32);
    void *adj3 = dlmalloc(32);  // prevent adj2 from merging into top
    printf("adj1=%p adj2=%p adj3=%p\n", adj1, adj2, adj3);
    dlfree(adj2);  // now the chunk after adj1 is free
    printf("Freed adj2, now realloc adj1 to 64 bytes...\n");
    void *adj_result = dlrealloc(adj1, 64);
    printf("Realloc'd adj1 to 64 bytes at %p\n", adj_result);
    dlfree(adj_result);
    dlfree(adj3);
    printf("========================== END REALLOC 3 ==========================\n");


    printf("======================= START MTE ALLOC %d =======================\n", ++i);
    void *tiny_ptr3 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr3);
    printf("========================== END ALLOC %d ==========================\n", i);

    // printf("======================= START MTE ALLOC %d =======================\n", ++i);
    // num_bytes = 4096;
    // void *tiny_ptr4 = dlmalloc(num_bytes);
    // printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr4);
    // printf("========================== END ALLOC %d ==========================\n", i);

    // printf("======================= START MTE ALLOC %d =======================\n", ++i);
    // num_bytes = 262144;
    // void *tiny_ptr5 = dlmalloc(num_bytes);
    // printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr5);
    // printf("========================== END ALLOC %d ==========================\n", i);

    // printf("======================= START MTE ALLOC %d =======================\n", ++i);
    // void *tiny_ptr6 = dlmalloc(num_bytes);
    // printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr6);
    // printf("========================== END ALLOC %d ==========================\n", i);

    // printf("======================= START MTE ALLOC %d =======================\n", ++i);
    // void *tiny_ptr7 = dlmalloc(num_bytes);
    // printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr7);
    // printf("========================== END ALLOC %d ==========================\n", i);

    

    /* THE FOLLOWING ARE COMMENTED OUT BECAUASE THEY WILL CRASH (GOOD) */

    // printf("TEST: Freed tiny_ptr. Attempting stale read...\n");
    // volatile uint8_t uaf_val = ((volatile uint8_t *)tiny_ptr)[0];
    // printf("!! UAF read succeeded (BUG NOT CAUGHT): %d\n", uaf_val);

    // void *reuse_tiny_ptr = dlmalloc(16);
    // printf("!! \t(Reuse) Allocated %d bytes of memory at %p\n", (int)num_bytes, reuse_tiny_ptr);
    // dlfree(reuse_tiny_ptr);

    // printf("TEST: Flip one tag bit...\n");
    // volatile uint8_t *bad = (uint8_t *)((uintptr_t)tiny_ptr2 ^ (1UL << 56));
    // (void)*bad; /* if SYNC+MTE works, this SIGSEGVs immediately with si_code == SEGV_MTESERR */
    // printf("!! Tag mod did not fault (BUG NOT CAUGHT): %p\n", bad);

    // printf("TEST: Overflow chunk to next chunk...\n");
    // ((char*)tiny_ptr3)[17] = 'A';
    // printf("!! Buffer overflow succeeded (BUG NOT CAUGHT)\n");

    // dlfree(tiny_ptr);
    dlfree(tiny_ptr2);
    dlfree(tiny_ptr3);
    // dlfree(tiny_ptr4);
    // dlfree(tiny_ptr5);
    // dlfree(tiny_ptr6);
    // dlfree(tiny_ptr7);

    return EXIT_SUCCESS;
}