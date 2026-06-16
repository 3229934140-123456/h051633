#include <stdlib.h>

#include "gc.h"
#include "vm.h"
#include "object.h"
#include "memory.h"
#include "compiler.h"

void gc_mark_object(Obj* object) {
    if (object == NULL) return;
    if (object->is_marked) return;

    object->is_marked = true;

    if (vm.gray_capacity < vm.gray_count + 1) {
        vm.gray_capacity = vm.gray_capacity < 8 ? 8 : vm.gray_capacity * 2;
        vm.gray_stack = (Obj**)realloc(vm.gray_stack,
                                       sizeof(Obj*) * vm.gray_capacity);
    }

    vm.gray_stack[vm.gray_count++] = object;
}

void gc_mark_value(Value value) {
    if (IS_OBJ(value)) {
        gc_mark_object(AS_OBJ(value));
    }
}

void gc_mark_table(Table* table) {
    if (table == NULL) return;
    for (int i = 0; i < table->capacity; i++) {
        TableEntry* entry = &table->entries[i];
        if (entry->key != NULL) {
            gc_mark_object(&entry->key->obj);
            gc_mark_value(entry->value);
        }
    }
}

void gc_mark_array(Value* array, int count) {
    for (int i = 0; i < count; i++) {
        gc_mark_value(array[i]);
    }
}

static void mark_roots() {
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
        gc_mark_value(*slot);
    }

    for (int i = 0; i < vm.frame_count; i++) {
        if (vm.frames[i].closure != NULL) {
            gc_mark_object(&vm.frames[i].closure->obj);
        }
    }

    for (ObjUpvalue* upvalue = vm.open_upvalues;
         upvalue != NULL;
         upvalue = upvalue->next) {
        gc_mark_object(&upvalue->obj);
    }

    gc_mark_table(&vm.globals);
    gc_mark_table(&vm.strings);

    compiler_mark_roots();
}

void gc_blacken_object(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    print_object(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_STRING:
            break;

        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            if (function->name != NULL) {
                gc_mark_object(&function->name->obj);
            }
            gc_mark_array(function->chunk.constants.data,
                          function->chunk.constants.count);
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (closure->function != NULL) {
                gc_mark_object(&closure->function->obj);
            }
            for (int i = 0; i < closure->upvalue_count; i++) {
                if (closure->upvalues[i] != NULL) {
                    gc_mark_object(&closure->upvalues[i]->obj);
                }
            }
            break;
        }

        case OBJ_UPVALUE:
            gc_mark_value(((ObjUpvalue*)object)->closed);
            break;

        case OBJ_NATIVE:
            break;

        case OBJ_TABLE: {
            ObjTable* table_obj = (ObjTable*)object;
            gc_mark_table(&table_obj->table);
            break;
        }

        case OBJ_USERDATA:
            break;
    }
}

static void trace_references() {
    while (vm.gray_count > 0) {
        Obj* object = vm.gray_stack[--vm.gray_count];
        gc_blacken_object(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            switch (unreached->type) {
                case OBJ_STRING: {
                    ObjString* str = (ObjString*)unreached;
                    FREE_ARRAY(char, str->chars, str->length + 1);
                    break;
                }
                case OBJ_FUNCTION: {
                    ObjFunction* fn = (ObjFunction*)unreached;
                    chunk_free(&fn->chunk);
                    break;
                }
                case OBJ_CLOSURE: {
                    ObjClosure* cl = (ObjClosure*)unreached;
                    FREE_ARRAY(ObjUpvalue*, cl->upvalues, cl->upvalue_count);
                    break;
                }
                case OBJ_UPVALUE:
                    break;
                case OBJ_NATIVE:
                    break;
                case OBJ_TABLE: {
                    ObjTable* t = (ObjTable*)unreached;
                    table_free(&t->table);
                    break;
                }
                case OBJ_USERDATA: {
                    ObjUserdata* ud = (ObjUserdata*)unreached;
                    if (ud->finalizer) {
                        ud->finalizer(ud->data);
                    }
                    FREE_ARRAY(u8, ud->data, ud->size);
                    break;
                }
            }

            size_t size = unreached->size;
            FREE(Obj, unreached);
            vm.bytes_allocated -= size;
        }
    }
}

void gc_collect() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif

    if (!vm.gc_enabled) return;

    mark_roots();
    trace_references();
    table_remove_white(&vm.strings);
    sweep();

    vm.next_gc = vm.bytes_allocated * MJ_GC_HEAP_GROW_FACTOR;
    if (vm.next_gc < MJ_GC_INITIAL_THRESHOLD) {
        vm.next_gc = MJ_GC_INITIAL_THRESHOLD;
    }

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytes_allocated, before, vm.bytes_allocated,
           vm.next_gc);
#endif
}

void* gc_reallocate(void* ptr, size_t old_size, size_t new_size) {
    if (vm.max_memory > 0 && new_size > old_size) {
        size_t delta = new_size - old_size;
        if (vm.bytes_allocated + delta > vm.max_memory) {
            // Out of memory - set error state
            if (vm.error_message == NULL) {
                vm.error_message = "Out of memory: allocation exceeds memory limit.";
                vm.error_line = 0;
            }
            return NULL;
        }
    }

    vm.bytes_allocated += new_size - old_size;

    if (new_size > old_size) {
        if (vm.gc_enabled && vm.bytes_allocated > vm.next_gc) {
            gc_collect();
        }
    }

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* result = realloc(ptr, new_size);
    if (result == NULL) {
        // Allocation failed from system
        if (vm.error_message == NULL) {
            vm.error_message = "Out of memory: system allocation failed.";
            vm.error_line = 0;
        }
        return NULL;
    }
    return result;
}
