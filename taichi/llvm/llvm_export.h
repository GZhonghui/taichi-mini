// lib 的接口函数
// 个人习惯为每一个 lib 写这样一个专门用于导出函数的 C 模块

// 喜欢老式的头文件写法
#ifndef LLVM_EXPORT_H
#define LLVM_EXPORT_H

#include <algorithm>
#include <cstdint>

#include "llvm_manager.h"

// 本 lib 一律使用 uint8_t 类型传递 Bytes

extern "C" void init_lib();　// 初始化 lib
extern "C" void set_log_level(uint8_t level); // 设定 log level
// 开始一个函数定义
extern "C" void function_begin(
    uint8_t *function_name,
    uint8_t args_number,
    uint8_t *args_type,
    uint8_t *args_name,
    uint8_t return_type
);
// 函数定义完成
extern "C" void function_finish(
    uint8_t *function_name
);
// 开始一个循环定义
extern "C" void loop_begin(
    uint8_t *function_name,
    uint8_t *loop_index_name,
    int32_t l,
    int32_t r,
    int32_t s
);
// 结束一个循环定义
extern "C" void loop_finish(
    uint8_t *function_name
);
// 定义一个赋值语句（右侧为 Value）
extern "C" void assignment_statement_value(
    uint8_t *function_name,
    uint8_t *target_variable_name,
    uint8_t *source_buffer
);
// 定义一个赋值语句（右侧为简单运算表达式）
extern "C" void assignment_statement_operation(
    uint8_t *function_name,
    uint8_t *target_variable_name,
    uint8_t *left_buffer,
    uint8_t operation_type,
    uint8_t *right_buffer
);
// 定义一个返回语句
extern "C" void return_statement(
    uint8_t *function_name,
    uint8_t *return_variable_name
);
// 执行函数
// 出于学习目的保留，实际上未使用
extern "C" void run(
    uint8_t *function_name,
    uint8_t *argument_buffer,
    uint8_t *result_buffer
);
// 获取一个 LLVM 编译的 C 函数的原始指针
extern "C" void *get_func_ptr(
    uint8_t *function_name
);

#endif
