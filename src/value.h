#ifndef MJ_VALUE_H
#define MJ_VALUE_H

#include "common.h"

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define NIL_VAL         make_nil_val()
#define BOOL_VAL(value) make_bool_val(value)
#define NUMBER_VAL(num) make_number_val(num)
#define OBJ_VAL(obj)    make_obj_val((Obj*)obj)

static inline Value make_nil_val(void) {
    Value v;
    v.type = VAL_NIL;
    v.as.number = 0;
    return v;
}

static inline Value make_bool_val(bool b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b;
    return v;
}

static inline Value make_number_val(double n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

static inline Value make_obj_val(Obj* o) {
    Value v;
    v.type = VAL_OBJ;
    v.as.obj = o;
    return v;
}

#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)

bool values_equal(Value a, Value b);
void print_value(Value value);

typedef struct {
    int count;
    int capacity;
    Value* data;
} ValueArray;

void value_array_init(ValueArray* array);
void value_array_write(ValueArray* array, Value value);
void value_array_free(ValueArray* array);

#endif
