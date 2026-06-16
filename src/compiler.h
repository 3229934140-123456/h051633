#ifndef MJ_COMPILER_H
#define MJ_COMPILER_H

#include "common.h"
#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source, InterpretResult* result);

void compiler_mark_roots();

#endif
