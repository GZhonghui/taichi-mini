#ifndef LLVM_EXPORT_H
#define LLVM_EXPORT_H

#include <algorithm>
#include <cstdint>

#include "llvm_manager.h"

extern "C" void init_lib();
extern "C" void function_begin(
    uint8_t *function_name,
    uint8_t args_number,
    uint8_t *args_type,
    uint8_t *args_name,
    uint8_t return_type
);
extern "C" void function_finish();

#endif
