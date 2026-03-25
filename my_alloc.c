#include "my_alloc.h"

#include <sys/mman.h>   // mmap, munmap
#include <stddef.h>     // size_t, NULL
#include <stdint.h>     // uint8_t, uintptr_t
#include <string.h>     // memset (for zeroing)
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, abort()
#include <stdio.h>      // fprintf, stderr

#define PAGE_SIZE           4096
#define POOL_SIZE           65536
#define NUM_SIZE_CLASSES    9
#define MTE_GRANULE         16      // MTE tags at 16-byte granularity

int main() {
	const char * msg = "hello";
  printf("%s\n", msg);
	return 0;
}