#ifndef LLVM_EXPORT_H
#define LLVM_EXPORT_H

#include <algorithm>
#include <cstdint>

#include "llvm_manager.h"

extern "C" void init_lib();

// extern "C" void function_entry(uint8_t *function_name_buffer);
// extern "C" void function_exit();

// extern "C" void loop_entry(uint32_t l, uint32_t r, uint32_t s);
// extern "C" void loop_exit();

// extern "C" void assignment_statements();

#endif
