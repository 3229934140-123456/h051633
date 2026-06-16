#ifndef MJ_HOST_H
#define MJ_HOST_H

#include "common.h"
#include "value.h"
#include "vm.h"
#include "object.h"

typedef struct MjState MjState;

typedef enum {
    MJ_TYPE_NIL = 0,
    MJ_TYPE_BOOL,
    MJ_TYPE_NUMBER,
    MJ_TYPE_STRING,
    MJ_TYPE_TABLE,
    MJ_TYPE_FUNCTION,
    MJ_TYPE_NATIVE,
    MJ_TYPE_USERDATA,
    MJ_TYPE_CLOSURE
} MjType;

typedef Value (*MjCFunction)(int argc, Value* argv);

MjState* mj_open();
void mj_close(MjState* state);

void mj_set_max_memory(MjState* state, size_t bytes);
void mj_set_max_steps(MjState* state, int steps);

int mj_get_error(MjState* state, char** message);

int mj_load_string(MjState* state, const char* source);
int mj_call(MjState* state, int nargs, int nresults);

void mj_push_nil(MjState* state);
void mj_push_bool(MjState* state, bool value);
void mj_push_number(MjState* state, double value);
void mj_push_cfunction(MjState* state, MjCFunction fn, int arity, const char* name);
void mj_push_lstring(MjState* state, const char* s, size_t len);
void mj_push_string(MjState* state, const char* s);
void mj_push_value(MjState* state, Value value);

void mj_pop(MjState* state);
Value mj_to_value(MjState* state, int index);
bool mj_to_bool(MjState* state, int index);
double mj_to_number(MjState* state, int index);
const char* mj_to_string(MjState* state, int index, int* len);
void* mj_to_userdata(MjState* state, int index);

bool mj_is_nil(MjState* state, int index);
bool mj_is_bool(MjState* state, int index);
bool mj_is_number(MjState* state, int index);
bool mj_is_string(MjState* state, int index);
bool mj_is_table(MjState* state, int index);
bool mj_is_function(MjState* state, int index);
bool mj_is_userdata(MjState* state, int index);

MjType mj_type(MjState* state, int index);

void mj_set_global(MjState* state, const char* name);
void mj_get_global(MjState* state, const char* name);

void mj_new_table(MjState* state);
void mj_set_table(MjState* state, int table_index);
void mj_get_table(MjState* state, int table_index);
void mj_set_index(MjState* state, int table_index);
void mj_get_index(MjState* state, int table_index);

void* mj_new_userdata(MjState* state, size_t size, void (*finalizer)(void*));

int mj_get_top(MjState* state);

InterpretResult mj_result_code(MjState* state);

#endif
