#include "array.h"
#include <string.h>

#if WIN32
#ifndef aligned_alloc
#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#endif
#ifndef aligned_free
#define aligned_free _aligned_free
#endif
#endif

void array_init(array *a, uint32_t stride, uint32_t capacity) {
    assert(stride != 0 && capacity != 0);
    a->stride = stride;
    a->capacity = capacity;
    a->size = 0;
    const size_t size = stride * capacity;
    a->ptr = aligned_alloc(16, size);
    memset(a->ptr, 0, size);
}

void array_reserve(array *a, uint32_t new_capacity) {
    if (new_capacity <= a->capacity) return;
    void *ptr = aligned_alloc(16, a->stride * new_capacity);
    void *old_ptr = a->ptr;
    if (a->ptr) {
        memcpy(ptr, old_ptr, a->size * a->stride);
        aligned_free(old_ptr);
    }
    a->ptr = ptr;
    a->capacity = new_capacity;
}

void array_resize(array *a, uint32_t new_size) {
    array_reserve(a, new_size);
    a->size = new_size;
}

void array_free(array *a) {
    aligned_free(a->ptr);
    a->ptr = NULL;
    a->capacity = a->size = 0;
    // stride remains unchanged
}
