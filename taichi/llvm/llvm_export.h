#ifndef LLVM_EXPORT_H
#define LLVM_EXPORT_H

#include <algorithm>
#include <cstdint>

#include "llvm_manager.h"

extern "C" void init_lib();
extern "C" void set_log_level(uint8_t level);
extern "C" void function_begin(
    uint8_t *function_name,
    uint8_t args_number,
    uint8_t *args_type,
    uint8_t *args_name,
    uint8_t return_type
);
extern "C" void function_finish(
    uint8_t *function_name
);
extern "C" void loop_begin(
    uint8_t *function_name,
    uint8_t *loop_index_name,
    int32_t l,
    int32_t r,
    int32_t s
);
extern "C" void loop_finish(
    uint8_t *function_name
);
extern "C" void assignment_statement_value(
    uint8_t *function_name,
    uint8_t *target_variable_name,
    uint8_t *source_buffer
);
extern "C" void assignment_statement_operation(
    uint8_t *function_name,
    uint8_t *target_variable_name,
    uint8_t *left_buffer,
    uint8_t operation_type,
    uint8_t *right_buffer
);
extern "C" void return_statement(
    uint8_t *function_name,
    uint8_t *return_variable_name
);
extern "C" void run(
    uint8_t *function_name,
    uint8_t *argument_buffer,
    uint8_t *result_buffer
);
extern "C" void *get_func_ptr(
    uint8_t *function_name
);

#endif
