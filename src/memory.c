#include "common.h"
#include "gc.h"
#include "vm.h"

void* mj_reallocate(void* ptr, size_t old_size, size_t new_size) {
    return gc_reallocate(ptr, old_size, new_size);
}
