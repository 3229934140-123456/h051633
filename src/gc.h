#ifndef MJ_GC_H
#define MJ_GC_H

#include "common.h"
#include "table.h"

void gc_collect();
void gc_mark_value(Value value);
void gc_mark_object(Obj* object);
void gc_mark_table(Table* table);
void gc_mark_array(Value* array, int count);

void gc_gray_object(Obj* object);
void gc_blacken_object(Obj* object);

void* gc_reallocate(void* ptr, size_t old_size, size_t new_size);

#endif
