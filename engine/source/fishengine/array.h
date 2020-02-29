#ifndef ARRAY_H
#define ARRAY_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array array;

// std::vector in c++
struct array {
    void *ptr;
    uint32_t stride;
    uint32_t capacity;
    uint32_t size;
};

// note: call array_init on an already inited array will cause memory leak
void array_init(array *a, uint32_t stride, uint32_t capacity);
// note: do NOT these functions on an uninited array
void array_reserve(array *a, uint32_t new_capacity);
void array_resize(array *a, uint32_t new_size);
void array_free(array *a);

static inline void *array_at(array *a, uint32_t idx) {
    assert(idx >= 0 && idx < a->size);
    char *p = (char *)a->ptr + idx * a->stride;  // make c++ compiler happy
    return (void *)p;
}

static inline void *array_reverse_at(array *a, uint32_t idx) {
    assert(idx >= 0 && idx < a->size);
    char *p = (char *)a->ptr +
              (a->size - 1 - idx) * a->stride;  // make c++ compiler happy
    return (void *)p;
}

static inline void *array_push(array *a) {
    if (a->size >= a->capacity) {
        array_reserve(a, a->capacity * 2);
    }
    uint32_t i = a->size++;
    return array_at(a, i);
}

static inline uint32_t array_get_index(array *a, void *p) {
    uint32_t diff = (char *)p - (char *)a->ptr;
    assert(diff % a->stride == 0);
    assert(diff < a->stride * a->size);
    return diff / a->stride;
}

static inline uint32_t array_get_bytelength(array *a) {
    return a->size * a->stride;
}

#ifdef __cplusplus
}
#endif

#endif
