#ifndef MJ_OBJECT_H
#define MJ_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_NATIVE,
    OBJ_TABLE,
    OBJ_USERDATA
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
    size_t size;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    u32 hash;
};

typedef struct ObjFunction {
    Obj obj;
    int arity;
    int upvalue_count;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalue_count;
} ObjClosure;

typedef Value (*NativeFn)(int arg_count, Value* args);

typedef struct ObjNative {
    Obj obj;
    NativeFn function;
    int arity;
    const char* name;
} ObjNative;

typedef struct ObjTable {
    Obj obj;
    Table table;
} ObjTable;

typedef struct ObjUserdata {
    Obj obj;
    size_t size;
    void* data;
    void (*finalizer)(void* data);
} ObjUserdata;

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value)  is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_NATIVE(value)  is_obj_type(value, OBJ_NATIVE)
#define IS_TABLE(value)   is_obj_type(value, OBJ_TABLE)
#define IS_USERDATA(value) is_obj_type(value, OBJ_USERDATA)

#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE(value)  (((ObjNative*)AS_OBJ(value))->function)
#define AS_TABLE(value)   ((ObjTable*)AS_OBJ(value))
#define AS_USERDATA(value) ((ObjUserdata*)AS_OBJ(value))

static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjString* take_string(char* chars, int length);
ObjString* copy_string(const char* chars, int length);

ObjFunction* new_function();
ObjClosure* new_closure(ObjFunction* function);
ObjUpvalue* new_upvalue(Value* slot);
ObjNative* new_native(NativeFn function, int arity, const char* name);
ObjTable* new_table();
ObjUserdata* new_userdata(size_t size, void (*finalizer)(void*));

void print_object(Value value);

#endif
