#define USE_DL_PREFIX

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "malloc.h"

int main() {
    int i = 0;
    size_t num_bytes = 16;
    printf("===================== STARTING ALLOC %d ===================== \n", ++i);
    void *tiny_ptr = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr);
    printf("===================== END ALLOC %d ===================== \n", i);
    

    printf("===================== STARTING ALLOC %d ===================== \n", ++i);
    void *tiny_ptr2 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr2);
    printf("===================== END ALLOC %d ===================== \n", i);


    printf("===================== STARTING ALLOC %d ===================== \n", ++i);
    void *tiny_ptr3 = dlmalloc(num_bytes);
    printf("!! \tAllocated %d bytes of memory at %p\n", (int)num_bytes, tiny_ptr3);
    printf("===================== END ALLOC %d ===================== \n", i);


    dlfree(tiny_ptr);
    dlfree(tiny_ptr2);
    dlfree(tiny_ptr3);

    return EXIT_SUCCESS;
}