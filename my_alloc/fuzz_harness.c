#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string.h>
#include <stdio.h>

#include "my_alloc.h"

#ifdef MTE_ENABLED
    #define ALLOC(sz)   my_malloc_mte(sz)
    #define FREE(ptr)   my_free_mte(ptr)
#else
    #define ALLOC(sz)   my_malloc(sz)
    #define FREE(ptr)   my_free(ptr)
#endif

#define MAX_ALLOCS 16
#define STATS_INTERVAL 50000

/* Operations the fuzzer can pick */
enum op {
    OP_ALLOC,           // allocate a slot
    OP_FREE,            // free a slot
    OP_WRITE_VALID,     // write within bounds
    OP_WRITE_OVERFLOW,  // write past end of bounds
    OP_READ_AFTER_FREE, // read from freed
    OP_DOUBLE_FREE,     // free already-freed
    NUM_OPS
};

/* Global counters so we can verify bugs are actually firing */
static unsigned long long g_total_runs = 0;
static unsigned long long g_overflow_fired = 0;
static unsigned long long g_uaf_read_fired = 0;
static unsigned long long g_double_free_fired = 0;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Only 1 param passed in
    if (size < 2) return 0;

    my_alloc_init();

    void *slots[MAX_ALLOCS] = {0};
    int freed[MAX_ALLOCS] = {0};
    size_t alloc_size[MAX_ALLOCS] = {0};
    size_t pos = 0;

    while (pos + 2 <= size) {
        // Byte 1: pick operation
        uint8_t op = data[pos++] % NUM_OPS;
        // Byte 2: pick slot index and/or size
        uint8_t param = data[pos++];
        uint8_t slot = param % MAX_ALLOCS;

        switch (op) {
            case OP_ALLOC: {
                // Allocate with a size class derived from param
                size_t sz = (param % 8 + 1) * 16; // 16...128
                if (!slots[slot] || freed[slot]) {
                    slots[slot] = ALLOC(sz);
                    freed[slot] = 0;
                    alloc_size[slot] = sz;
                }
                break;
            }
            case OP_FREE: {
                if (slots[slot] && !freed[slot]) {
                    FREE(slots[slot]);
                    freed[slot] = 1;
                    // Keep stale pointer for buggy ops
                }
                break;
            }
            case OP_WRITE_VALID: {
                if (slots[slot] && !freed[slot]) {
                    ((uint8_t *)slots[slot])[0] = param;
                }
                break;
            }
            case OP_WRITE_OVERFLOW: {
                // Write past slot boundary
                if (slots[slot] && !freed[slot]) {
                    g_overflow_fired++;
                    size_t bad_offset = 16 + (param % 32);
                    ((volatile uint8_t *)slots[slot])[bad_offset] = 0xAA;
                }
                break;
            }
            case OP_READ_AFTER_FREE: {
                // Use after free read
                if (slots[slot] && freed[slot]) {
                    g_uaf_read_fired++;
                    volatile uint8_t val = ((volatile uint8_t *)slots[slot])[0];
                    (void)val;
                }
                break;
            }
            case OP_DOUBLE_FREE: {
                // Use an already freed pointer
                if (slots[slot] && freed[slot]) {
                    g_double_free_fired++;
                    FREE(slots[slot]);
                }
                break;
            }
        }
    }

    my_alloc_destroy();

    g_total_runs++;

    if (g_total_runs % STATS_INTERVAL == 0) {
        fprintf(stderr, "[STATS] runs=%llu overflow=%llu uaf_read=%llu "
                "double_free=%llu reuse_stale=%llu\n",
                g_total_runs, g_overflow_fired, g_uaf_read_fired,
                g_double_free_fired);
    }

    return 0;
}