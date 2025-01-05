#include "llvm_export.h"

#include <iostream>

void init_lib() {
    llvm_taichi::init();
}

void function_begin(
    uint8_t *function_name,
    uint8_t args_number,
    uint8_t *args_type,
    uint8_t *args_name,
    uint8_t return_type
) {
    std::string _m = std::string("compiling function ") + (char *)function_name + "...";
    Out::Log(pType::MESSAGE, _m.c_str());

}

void function_finish() {

}