#ifndef MJ_DEBUG_H
#define MJ_DEBUG_H

#include "chunk.h"

void chunk_disassemble(Chunk* chunk, const char* name);
int chunk_disasm_instruction(Chunk* chunk, int offset);

#endif
