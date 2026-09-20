#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MALLOC_CAP_8BIT (1 << 0)
#define MALLOC_CAP_INTERNAL (1 << 1)

static inline void *heap_caps_malloc(size_t size, uint32_t caps) {
    (void)caps;
    return calloc(1, size);
}

static inline size_t heap_caps_get_free_size(uint32_t caps) {
    (void)caps;
    return 100000;
}

static inline size_t heap_caps_get_minimum_free_size(uint32_t caps) {
    (void)caps;
    return 100000;
}

static inline size_t heap_caps_get_largest_free_block(uint32_t caps) {
    (void)caps;
    return 100000;
}
