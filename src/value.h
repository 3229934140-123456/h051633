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

#define NIL_VAL         ((Value){VAL_NIL, {.number = 0}})
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER_VAL(num) ((Value){VAL_NUMBER, {.number = (num)}})
#define OBJ_VAL(obj)    ((Value){VAL_OBJ, {.obj = (Obj*)obj}})

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
