#ifndef MJ_COMMON_H
#define MJ_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define MJ_VERSION "0.1.0"

#ifndef DEBUG
#define DEBUG 0
#endif

#define DEBUG_PRINT(code) do { if (DEBUG) { code; } } while (0)

#define MJ_UNUSED(x) ((void)(x))

#define MJ_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define MJ_GROW_ARRAY(type, ptr, old_count, new_count) \
    ((type*)mj_reallocate((ptr), sizeof(type) * (old_count), \
                          sizeof(type) * (new_count)))

#define MJ_FREE_ARRAY(type, ptr, count) \
    (mj_reallocate((ptr), sizeof(type) * (count), 0))

#define MJ_ALLOCATE(type, count) \
    ((type*)mj_reallocate(NULL, 0, sizeof(type) * (count)))

#define MJ_FREE(type, ptr) \
    (mj_reallocate(ptr, sizeof(type), 0))

#define GROW_CAPACITY(capacity) MJ_GROW_CAPACITY(capacity)
#define GROW_ARRAY(type, ptr, old, new) MJ_GROW_ARRAY(type, ptr, old, new)
#define FREE_ARRAY(type, ptr, count) MJ_FREE_ARRAY(type, ptr, count)
#define ALLOCATE(type, count) MJ_ALLOCATE(type, count)
#define FREE(type, ptr) MJ_FREE(type, ptr)
#define REALLOC(type, ptr, old_count, new_count) \
    ((type*)mj_reallocate(ptr, sizeof(type) * (old_count), sizeof(type) * (new_count)))

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

void* mj_reallocate(void* ptr, size_t old_size, size_t new_size);

#endif
