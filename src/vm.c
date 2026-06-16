#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "gc.h"

VM vm;

static InterpretResult run(int target_frames);

static Value print_native(int arg_count, Value* args) {
    for (int i = 0; i < arg_count; i++) {
        if (i > 0) printf(" ");
        print_value(args[i]);
    }
    printf("\n");
    return NIL_VAL;
}

static Value clock_native(int arg_count, Value* args) {
    MJ_UNUSED(arg_count);
    MJ_UNUSED(args);
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value len_native(int arg_count, Value* args) {
    if (arg_count != 1) {
        return NIL_VAL;
    }
    if (IS_STRING(args[0])) {
        return NUMBER_VAL(AS_STRING(args[0])->length);
    }
    return NIL_VAL;
}

static Value type_native(int arg_count, Value* args) {
    if (arg_count != 1) return NIL_VAL;
    switch (args[0].type) {
        case VAL_NIL: return OBJ_VAL(copy_string("nil", 3));
        case VAL_BOOL: return OBJ_VAL(copy_string("bool", 4));
        case VAL_NUMBER: return OBJ_VAL(copy_string("number", 6));
        case VAL_OBJ:
            switch (OBJ_TYPE(args[0])) {
                case OBJ_STRING: return OBJ_VAL(copy_string("string", 6));
                case OBJ_FUNCTION:
                case OBJ_CLOSURE: return OBJ_VAL(copy_string("function", 8));
                case OBJ_NATIVE: return OBJ_VAL(copy_string("native", 6));
                case OBJ_TABLE: return OBJ_VAL(copy_string("table", 5));
                case OBJ_USERDATA: return OBJ_VAL(copy_string("userdata", 8));
                default: return OBJ_VAL(copy_string("object", 6));
            }
    }
    return NIL_VAL;
}

static void reset_stack() {
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

void vm_push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

Value vm_pop() {
    vm.stack_top--;
    return *vm.stack_top;
}

Value vm_peek(int distance) {
    return vm.stack_top[-1 - distance];
}

static bool call_value(Value callee, int arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return vm_call_function(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                vm_push(result);
                return true;
            }
            default: break;
        }
    }
    vm_runtime_error("Can only call functions and classes.");
    return false;
}

bool vm_call_function(ObjClosure* closure, int arg_count) {
    if (arg_count != closure->function->arity && closure->function->arity != -1) {
        vm_runtime_error("Expected %d arguments but got %d.",
                         closure->function->arity, arg_count);
        return false;
    }

    if (vm.frame_count == MJ_FRAMES_MAX) {
        vm_runtime_error("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

InterpretResult vm_call(int arg_count) {
    Value callee = vm_peek(arg_count);

    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                vm_push(result);
                return RESULT_OK;
            }
            case OBJ_CLOSURE: {
                int target_frames = vm.frame_count;
                if (!vm_call_function(AS_CLOSURE(callee), arg_count)) {
                    return RESULT_RUNTIME_ERROR;
                }
                return run(target_frames);
            }
            default: break;
        }
    }
    vm_runtime_error("Can only call functions and classes.");
    return RESULT_RUNTIME_ERROR;
}

static ObjUpvalue* capture_upvalue(Value* local) {
    ObjUpvalue* prev_upvalue = NULL;
    ObjUpvalue* upvalue = vm.open_upvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* created_upvalue = new_upvalue(local);
    created_upvalue->next = upvalue;

    if (prev_upvalue == NULL) {
        vm.open_upvalues = created_upvalue;
    } else {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(Value* last) {
    while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
        ObjUpvalue* upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static bool is_falsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
    ObjString* b = AS_STRING(vm_pop());
    ObjString* a = AS_STRING(vm_pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    vm_push(OBJ_VAL(result));
}

void vm_runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);

    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    int len = strlen(buf);
    vm.error_message = REALLOC(char, vm.error_message, 0, len + 1);
    memcpy(vm.error_message, buf, len + 1);

    if (vm.frame_count > 0) {
        CallFrame* frame = &vm.frames[vm.frame_count - 1];
        int instruction = (int)(frame->ip - frame->closure->function->chunk.code - 1);
        vm.error_line = frame->closure->function->chunk.lines[instruction];
    }

    reset_stack();
}

Value vm_stack_at(int index) {
    return vm.stack[index];
}

int vm_stack_size() {
    return (int)(vm.stack_top - vm.stack);
}

static InterpretResult run(int target_frames) {
    CallFrame* frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.data[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(vm_peek(0)) || !IS_NUMBER(vm_peek(1))) { \
            vm_runtime_error("Operands must be numbers."); \
            return RESULT_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(vm_pop()); \
        double a = AS_NUMBER(vm_pop()); \
        vm_push(value_type(a op b)); \
    } while (false)

    for (;;) {
        // Check for pending errors (e.g., out of memory during allocation)
        if (vm.error_message != NULL) {
            return RESULT_OUT_OF_MEMORY;
        }

        vm.execution_steps++;
        if (vm.max_execution_steps > 0 && vm.execution_steps >= vm.max_execution_steps) {
            vm_runtime_error("Execution timeout: too many steps.");
            return RESULT_TIMEOUT;
        }

#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        chunk_disasm_instruction(&frame->closure->function->chunk,
                                (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        u8 instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                vm_push(constant);
                break;
            }
            case OP_NIL: vm_push(NIL_VAL); break;
            case OP_TRUE: vm_push(BOOL_VAL(true)); break;
            case OP_FALSE: vm_push(BOOL_VAL(false)); break;
            case OP_POP: vm_pop(); break;
            case OP_DUP: vm_push(vm_peek(0)); break;

            case OP_GET_LOCAL: {
                u8 slot = READ_BYTE();
                vm_push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                u8 slot = READ_BYTE();
                frame->slots[slot] = vm_peek(0);
                break;
            }

            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!table_get(&vm.globals, name, &value)) {
                    vm_runtime_error("Undefined variable '%s'.", name->chars);
                    return RESULT_RUNTIME_ERROR;
                }
                vm_push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, vm_peek(0));
                vm_pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (table_set(&vm.globals, name, vm_peek(0))) {
                    table_delete(&vm.globals, name);
                    vm_runtime_error("Undefined variable '%s'.", name->chars);
                    return RESULT_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GET_UPVALUE: {
                u8 slot = READ_BYTE();
                vm_push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                u8 slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = vm_peek(0);
                break;
            }

            case OP_EQUAL: {
                Value b = vm_pop();
                Value a = vm_pop();
                vm_push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(vm_peek(0)) && IS_STRING(vm_peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(vm_peek(0)) && IS_NUMBER(vm_peek(1))) {
                    double b = AS_NUMBER(vm_pop());
                    double a = AS_NUMBER(vm_pop());
                    vm_push(NUMBER_VAL(a + b));
                } else {
                    vm_runtime_error("Operands must be two numbers or two strings.");
                    return RESULT_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_MOD: {
                if (!IS_NUMBER(vm_peek(0)) || !IS_NUMBER(vm_peek(1))) {
                    vm_runtime_error("Operands must be numbers.");
                    return RESULT_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(vm_pop());
                double a = AS_NUMBER(vm_pop());
                vm_push(NUMBER_VAL((double)((long)a % (long)b)));
                break;
            }

            case OP_NOT:
                vm_push(BOOL_VAL(is_falsey(vm_pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(vm_peek(0))) {
                    vm_runtime_error("Operand must be a number.");
                    return RESULT_RUNTIME_ERROR;
                }
                vm_push(NUMBER_VAL(-AS_NUMBER(vm_pop())));
                break;

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(vm_peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_SHORT();
                if (!is_falsey(vm_peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OP_CALL: {
                u8 arg_count = READ_BYTE();
                Value callee = vm_peek(arg_count);
                if (!call_value(callee, arg_count)) {
                    return RESULT_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = new_closure(function);
                vm_push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalue_count; i++) {
                    u8 is_local = READ_BYTE();
                    u8 index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(&frame->slots[index]);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                close_upvalues(vm.stack_top - 1);
                vm_pop();
                break;

            case OP_RETURN: {
                Value result = vm_pop();
                close_upvalues(frame->slots);
                vm.frame_count--;

                if (vm.frame_count == target_frames) {
                    if (target_frames == 0) {
                        vm_pop();
                    } else {
                        vm.stack_top = frame->slots;
                        vm_push(result);
                    }
                    return RESULT_OK;
                }

                vm.stack_top = frame->slots;
                vm_push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            case OP_NEW_TABLE: {
                ObjTable* t = new_table();
                if (t == NULL) return RESULT_OUT_OF_MEMORY;
                vm_push(OBJ_VAL(t));
                break;
            }

            case OP_GET_PROPERTY: {
                ObjString* name = READ_STRING();
                Value obj = vm_peek(0);
                if (!IS_TABLE(obj)) {
                    vm_runtime_error("Only tables have properties.");
                    return RESULT_RUNTIME_ERROR;
                }
                Value value;
                if (!table_get(&AS_TABLE(obj)->table, name, &value)) {
                    vm_runtime_error("Undefined property '%s'.", name->chars);
                    return RESULT_RUNTIME_ERROR;
                }
                vm_pop();
                vm_push(value);
                break;
            }
            case OP_SET_PROPERTY: {
                ObjString* name = READ_STRING();
                Value obj = vm_peek(1);
                if (!IS_TABLE(obj)) {
                    vm_runtime_error("Only tables have properties.");
                    return RESULT_RUNTIME_ERROR;
                }
                table_set(&AS_TABLE(obj)->table, name, vm_peek(0));
                Value val = vm_pop();
                vm_pop();
                vm_push(val);
                break;
            }

            case OP_GET_INDEX: {
                Value index = vm_pop();
                Value obj = vm_pop();
                if (IS_TABLE(obj) && IS_STRING(index)) {
                    Value value;
                    if (table_get(&AS_TABLE(obj)->table, AS_STRING(index), &value)) {
                        vm_push(value);
                    } else {
                        vm_push(NIL_VAL);
                    }
                } else {
                    vm_runtime_error("Invalid index operation.");
                    return RESULT_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_INDEX: {
                Value value = vm_pop();
                Value index = vm_pop();
                Value obj = vm_pop();
                if (IS_TABLE(obj) && IS_STRING(index)) {
                    table_set(&AS_TABLE(obj)->table, AS_STRING(index), value);
                    vm_push(value);
                } else {
                    vm_runtime_error("Invalid index assignment.");
                    return RESULT_RUNTIME_ERROR;
                }
                break;
            }

            case OP_LENGTH: {
                Value obj = vm_pop();
                if (IS_STRING(obj)) {
                    vm_push(NUMBER_VAL(AS_STRING(obj)->length));
                } else if (IS_TABLE(obj)) {
                    vm_push(NUMBER_VAL(AS_TABLE(obj)->table.count));
                } else {
                    vm_runtime_error("Cannot get length of this type.");
                    return RESULT_RUNTIME_ERROR;
                }
                break;
            }

            case OP_TYPEOF: {
                Value value = vm_pop();
                Value result = type_native(1, &value);
                vm_push(result);
                break;
            }

            case OP_ASSERT: {
                Value value = vm_pop();
                if (is_falsey(value)) {
                    vm_runtime_error("Assertion failed.");
                    return RESULT_RUNTIME_ERROR;
                }
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

void vm_define_native(const char* name, NativeFn function, int arity) {
    vm_push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    vm_push(OBJ_VAL(new_native(function, arity, name)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    vm_pop();
    vm_pop();
}

InterpretResult vm_interpret(const char* source) {
    InterpretResult compile_result;
    ObjFunction* function = compile(source, &compile_result);
    if (function == NULL) return compile_result;

    vm_push(OBJ_VAL(function));
    ObjClosure* closure = new_closure(function);
    vm_pop();
    vm_push(OBJ_VAL(closure));

    if (!call_value(OBJ_VAL(closure), 0)) {
        return RESULT_RUNTIME_ERROR;
    }

    return run(0);
}

void vm_init() {
    reset_stack();
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_gc = MJ_GC_INITIAL_THRESHOLD;
    vm.gc_enabled = true;
    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;
    vm.open_upvalues = NULL;
    vm.error_message = NULL;
    vm.error_line = 0;
    vm.execution_steps = 0;
    vm.max_execution_steps = 0;
    vm.max_memory = 0;

    table_init(&vm.globals);
    table_init(&vm.strings);

    vm_define_native("print", print_native, -1);
    vm_define_native("clock", clock_native, 0);
    vm_define_native("len", len_native, 1);
    vm_define_native("type", type_native, 1);
}

void vm_free() {
    if (vm.error_message) {
        free(vm.error_message);
        vm.error_message = NULL;
    }
    if (vm.gray_stack) {
        FREE_ARRAY(Obj*, vm.gray_stack, vm.gray_capacity);
    }
    vm.gc_enabled = false;
    gc_collect();
    table_free(&vm.globals);
    table_free(&vm.strings);
}
