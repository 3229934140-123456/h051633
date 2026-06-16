#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "common.h"

void value_array_init(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->data = NULL;
}

void value_array_write(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = MJ_GROW_CAPACITY(old_capacity);
        array->data = MJ_GROW_ARRAY(Value, array->data, old_capacity, array->capacity);
    }
    array->data[array->count] = value;
    array->count++;
}

void value_array_free(ValueArray* array) {
    MJ_FREE_ARRAY(Value, array->data, array->capacity);
    value_array_init(array);
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        default: return false;
    }
}

void print_value(Value value) {
    switch (value.type) {
        case VAL_NIL: printf("nil"); break;
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: print_object(value); break;
    }
}
