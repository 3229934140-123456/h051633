#ifndef MJ_CHUNK_H
#define MJ_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MOD,
    OP_NOT,
    OP_NEGATE,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_NEW_TABLE,
    OP_LENGTH,
    OP_TYPEOF,
    OP_ASSERT
} OpCode;

typedef struct {
    int count;
    int capacity;
    u8* code;
    int* lines;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk* chunk);
void chunk_write(Chunk* chunk, u8 byte, int line);
void chunk_free(Chunk* chunk);
int  chunk_add_constant(Chunk* chunk, Value value);

int chunk_disasm_instruction(Chunk* chunk, int offset);
void chunk_disassemble(Chunk* chunk, const char* name);

#endif
