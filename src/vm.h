#ifndef MJ_VM_H
#define MJ_VM_H

#include "common.h"
#include "value.h"
#include "object.h"
#include "chunk.h"

#define MJ_FRAMES_MAX 64
#define MJ_STACK_MAX  (MJ_FRAMES_MAX * 256)
#define MJ_GC_HEAP_GROW_FACTOR 2
#define MJ_GC_INITIAL_THRESHOLD (1024 * 1024)

typedef struct CallFrame {
    ObjClosure* closure;
    u8* ip;
    Value* slots;
} CallFrame;

typedef enum {
    RESULT_OK,
    RESULT_RUNTIME_ERROR,
    RESULT_COMPILE_ERROR,
    RESULT_OUT_OF_MEMORY,
    RESULT_STACK_OVERFLOW,
    RESULT_TIMEOUT
} InterpretResult;

typedef struct VM {
    CallFrame frames[MJ_FRAMES_MAX];
    int frame_count;

    Value stack[MJ_STACK_MAX];
    Value* stack_top;

    ObjUpvalue* open_upvalues;

    Obj* objects;

    Table globals;
    Table strings;

    size_t bytes_allocated;
    size_t next_gc;
    bool gc_enabled;

    int gray_count;
    int gray_capacity;
    Obj** gray_stack;

    int execution_steps;
    int max_execution_steps;
    size_t max_memory;

    char* error_message;
    int error_line;
} VM;

extern VM vm;

void vm_init();
void vm_free();

InterpretResult vm_interpret(const char* source);
bool vm_call_function(ObjClosure* closure, int arg_count);
InterpretResult vm_call(int arg_count);

void vm_push(Value value);
Value vm_pop();
Value vm_peek(int distance);
void vm_runtime_error(const char* format, ...);

void vm_define_native(const char* name, NativeFn function, int arity);

Value vm_stack_at(int index);
int vm_stack_size();

#endif
