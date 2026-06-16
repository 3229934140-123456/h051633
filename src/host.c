#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "vm.h"
#include "object.h"
#include "table.h"
#include "compiler.h"

struct MjState {
    int last_result;
};

MjState* mj_open() {
    MjState* state = (MjState*)malloc(sizeof(MjState));
    if (state == NULL) return NULL;
    state->last_result = 0;
    vm_init();
    return state;
}

void mj_close(MjState* state) {
    if (state) {
        vm_free();
        free(state);
    }
}

void mj_set_max_memory(MjState* state, size_t bytes) {
    MJ_UNUSED(state);
    vm.max_memory = bytes;
}

void mj_set_max_steps(MjState* state, int steps) {
    MJ_UNUSED(state);
    vm.max_execution_steps = steps;
    vm.execution_steps = 0;
}

int mj_get_error(MjState* state, char** message) {
    MJ_UNUSED(state);
    if (vm.error_message) {
        if (message) *message = vm.error_message;
        return vm.error_line;
    }
    if (message) *message = NULL;
    return 0;
}

int mj_load_string(MjState* state, const char* source) {
    InterpretResult result = vm_interpret(source);
    state->last_result = result;
    return result;
}

int mj_call(MjState* state, int nargs, int nresults) {
    MJ_UNUSED(nresults);

    int stack_size = vm_stack_size();
    if (stack_size < nargs + 1) {
        vm_runtime_error("Not enough values on stack for call.");
        state->last_result = RESULT_RUNTIME_ERROR;
        return RESULT_RUNTIME_ERROR;
    }

    Value callee = vm_stack_at(stack_size - nargs - 1);

    if (IS_FUNCTION(callee)) {
        ObjClosure* closure = new_closure(AS_FUNCTION(callee));
        int idx = stack_size - nargs - 1;
        vm.stack[idx] = OBJ_VAL(closure);
    }

    InterpretResult res = vm_call(nargs);
    state->last_result = res;
    return res;
}

void mj_push_nil(MjState* state) {
    MJ_UNUSED(state);
    vm_push(NIL_VAL);
}

void mj_push_bool(MjState* state, bool value) {
    MJ_UNUSED(state);
    vm_push(BOOL_VAL(value));
}

void mj_push_number(MjState* state, double value) {
    MJ_UNUSED(state);
    vm_push(NUMBER_VAL(value));
}

void mj_push_cfunction(MjState* state, MjCFunction fn, int arity, const char* name) {
    MJ_UNUSED(state);
    vm_push(OBJ_VAL(new_native(fn, arity, name)));
}

void mj_push_lstring(MjState* state, const char* s, size_t len) {
    MJ_UNUSED(state);
    vm_push(OBJ_VAL(copy_string(s, (int)len)));
}

void mj_push_string(MjState* state, const char* s) {
    mj_push_lstring(state, s, strlen(s));
}

void mj_push_value(MjState* state, Value value) {
    MJ_UNUSED(state);
    vm_push(value);
}

void mj_pop(MjState* state) {
    MJ_UNUSED(state);
    if (vm_stack_size() > 0) {
        vm_pop();
    }
}

static Value* index_to_addr(MjState* state, int index) {
    MJ_UNUSED(state);
    if (index >= 0) {
        return &vm.stack[index];
    } else {
        return vm.stack_top + index;
    }
}

Value mj_to_value(MjState* state, int index) {
    return *index_to_addr(state, index);
}

bool mj_to_bool(MjState* state, int index) {
    Value v = *index_to_addr(state, index);
    if (IS_NIL(v)) return false;
    if (IS_BOOL(v)) return AS_BOOL(v);
    if (IS_NUMBER(v)) return AS_NUMBER(v) != 0;
    if (IS_STRING(v)) return AS_STRING(v)->length > 0;
    return true;
}

double mj_to_number(MjState* state, int index) {
    Value v = *index_to_addr(state, index);
    if (IS_NUMBER(v)) return AS_NUMBER(v);
    return 0;
}

const char* mj_to_string(MjState* state, int index, int* len) {
    Value v = *index_to_addr(state, index);
    if (IS_STRING(v)) {
        ObjString* s = AS_STRING(v);
        if (len) *len = s->length;
        return s->chars;
    }
    if (len) *len = 0;
    return NULL;
}

void* mj_to_userdata(MjState* state, int index) {
    Value v = *index_to_addr(state, index);
    if (IS_USERDATA(v)) {
        return AS_USERDATA(v)->data;
    }
    return NULL;
}

bool mj_is_nil(MjState* state, int index) {
    return IS_NIL(*index_to_addr(state, index));
}

bool mj_is_bool(MjState* state, int index) {
    return IS_BOOL(*index_to_addr(state, index));
}

bool mj_is_number(MjState* state, int index) {
    return IS_NUMBER(*index_to_addr(state, index));
}

bool mj_is_string(MjState* state, int index) {
    return IS_STRING(*index_to_addr(state, index));
}

bool mj_is_table(MjState* state, int index) {
    return IS_TABLE(*index_to_addr(state, index));
}

bool mj_is_function(MjState* state, int index) {
    Value v = *index_to_addr(state, index);
    return IS_CLOSURE(v) || IS_NATIVE(v) || IS_FUNCTION(v);
}

bool mj_is_userdata(MjState* state, int index) {
    return IS_USERDATA(*index_to_addr(state, index));
}

MjType mj_type(MjState* state, int index) {
    Value v = *index_to_addr(state, index);
    switch (v.type) {
        case VAL_NIL: return MJ_TYPE_NIL;
        case VAL_BOOL: return MJ_TYPE_BOOL;
        case VAL_NUMBER: return MJ_TYPE_NUMBER;
        case VAL_OBJ:
            switch (OBJ_TYPE(v)) {
                case OBJ_STRING: return MJ_TYPE_STRING;
                case OBJ_TABLE: return MJ_TYPE_TABLE;
                case OBJ_FUNCTION: return MJ_TYPE_FUNCTION;
                case OBJ_CLOSURE: return MJ_TYPE_CLOSURE;
                case OBJ_NATIVE: return MJ_TYPE_NATIVE;
                case OBJ_USERDATA: return MJ_TYPE_USERDATA;
                default: return MJ_TYPE_NIL;
            }
        default: return MJ_TYPE_NIL;
    }
}

void mj_set_global(MjState* state, const char* name) {
    MJ_UNUSED(state);
    ObjString* key = copy_string(name, (int)strlen(name));
    Value value = vm_pop();
    table_set(&vm.globals, key, value);
}

void mj_get_global(MjState* state, const char* name) {
    MJ_UNUSED(state);
    ObjString* key = copy_string(name, (int)strlen(name));
    Value value;
    if (table_get(&vm.globals, key, &value)) {
        vm_push(value);
    } else {
        vm_push(NIL_VAL);
    }
}

void mj_new_table(MjState* state) {
    MJ_UNUSED(state);
    vm_push(OBJ_VAL(new_table()));
}

void mj_set_table(MjState* state, int table_index) {
    MJ_UNUSED(state);
    Value table_val = *index_to_addr(state, table_index);
    Value value = vm_pop();
    Value key = vm_pop();
    if (IS_TABLE(table_val) && IS_STRING(key)) {
        table_set(&AS_TABLE(table_val)->table, AS_STRING(key), value);
    }
}

void mj_get_table(MjState* state, int table_index) {
    MJ_UNUSED(state);
    Value table_val = *index_to_addr(state, table_index);
    Value key = vm_pop();
    if (IS_TABLE(table_val) && IS_STRING(key)) {
        Value value;
        if (table_get(&AS_TABLE(table_val)->table, AS_STRING(key), &value)) {
            vm_push(value);
            return;
        }
    }
    vm_push(NIL_VAL);
}

void mj_set_index(MjState* state, int table_index) {
    mj_set_table(state, table_index);
}

void mj_get_index(MjState* state, int table_index) {
    mj_get_table(state, table_index);
}

void* mj_new_userdata(MjState* state, size_t size, void (*finalizer)(void*)) {
    MJ_UNUSED(state);
    ObjUserdata* ud = new_userdata(size, finalizer);
    vm_push(OBJ_VAL(ud));
    return ud->data;
}

int mj_get_top(MjState* state) {
    MJ_UNUSED(state);
    return vm_stack_size();
}

InterpretResult mj_result_code(MjState* state) {
    return (InterpretResult)state->last_result;
}
