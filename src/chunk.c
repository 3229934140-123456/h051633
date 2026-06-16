#include <stdio.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void chunk_init(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    value_array_init(&chunk->constants);
}

void chunk_write(Chunk* chunk, u8 byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = MJ_GROW_CAPACITY(old_capacity);
        chunk->code = MJ_GROW_ARRAY(u8, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = MJ_GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void chunk_free(Chunk* chunk) {
    MJ_FREE_ARRAY(u8, chunk->code, chunk->capacity);
    MJ_FREE_ARRAY(int, chunk->lines, chunk->capacity);
    value_array_free(&chunk->constants);
    chunk_init(chunk);
}

int chunk_add_constant(Chunk* chunk, Value value) {
    value_array_write(&chunk->constants, value);
    return chunk->constants.count - 1;
}

static int simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char* name, Chunk* chunk, int offset) {
    u8 constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.data[constant]);
    printf("'\n");
    return offset + 2;
}

static int byte_instruction(const char* name, Chunk* chunk, int offset) {
    u8 slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char* name, int sign, Chunk* chunk, int offset) {
    u16 jump = (u16)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
           offset + 3 + sign * jump);
    return offset + 3;
}

int chunk_disasm_instruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    u8 instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simple_instruction("OP_NIL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_DUP:
            return simple_instruction("OP_DUP", offset);
        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE:
            return byte_instruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byte_instruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY:
            return constant_instruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return constant_instruction("OP_SET_PROPERTY", chunk, offset);
        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);
        case OP_LESS:
            return simple_instruction("OP_LESS", offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);
        case OP_MOD:
            return simple_instruction("OP_MOD", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_TRUE:
            return jump_instruction("OP_JUMP_IF_TRUE", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
        case OP_CLOSURE:
            return constant_instruction("OP_CLOSURE", chunk, offset);
        case OP_CLOSE_UPVALUE:
            return simple_instruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_NEW_TABLE:
            return simple_instruction("OP_NEW_TABLE", offset);
        case OP_LENGTH:
            return simple_instruction("OP_LENGTH", offset);
        case OP_TYPEOF:
            return simple_instruction("OP_TYPEOF", offset);
        case OP_ASSERT:
            return simple_instruction("OP_ASSERT", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void chunk_disassemble(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = chunk_disasm_instruction(chunk, offset);
    }
}
