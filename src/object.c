#include <stdio.h>
#include <string.h>

#include "object.h"
#include "value.h"
#include "memory.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

static Obj* allocate_object(size_t size, ObjType type) {
    Obj* object = (Obj*)mj_reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    object->size = size;
    object->next = vm.objects;
    vm.objects = object;

    if (vm.bytes_allocated > vm.next_gc && vm.gc_enabled) {
        gc_collect();
    }

    return object;
}

static u32 hash_string(const char* key, int length) {
    u32 hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (u8)key[i];
        hash *= 16777619;
    }
    return hash;
}

static ObjString* allocate_string(char* chars, int length, u32 hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    table_set(vm.strings, string, NIL_VAL);
    return string;
}

ObjString* take_string(char* chars, int length) {
    u32 hash = hash_string(chars, length);
    ObjString* interned = table_find_string(vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocate_string(chars, length, hash);
}

ObjString* copy_string(const char* chars, int length) {
    u32 hash = hash_string(chars, length);
    ObjString* interned = table_find_string(vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(heap_chars, length, hash);
}

ObjFunction* new_function() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    chunk_init(&function->chunk);
    return function;
}

ObjClosure* new_closure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

ObjUpvalue* new_upvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NIL_VAL;
    upvalue->next = NULL;
    return upvalue;
}

ObjNative* new_native(NativeFn function, int arity, const char* name) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    native->arity = arity;
    native->name = name;
    return native;
}

ObjTable* new_table() {
    ObjTable* table = ALLOCATE_OBJ(ObjTable, OBJ_TABLE);
    table_init(&table->table);
    return table;
}

ObjUserdata* new_userdata(size_t size, void (*finalizer)(void*)) {
    ObjUserdata* ud = ALLOCATE_OBJ(ObjUserdata, OBJ_USERDATA);
    ud->size = size;
    ud->finalizer = finalizer;
    ud->data = ALLOCATE(u8, size);
    memset(ud->data, 0, size);
    return ud;
}

static void print_function(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

void print_object(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(value)->function);
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        case OBJ_TABLE:
            printf("<table>");
            break;
        case OBJ_USERDATA:
            printf("<userdata>");
            break;
    }
}
