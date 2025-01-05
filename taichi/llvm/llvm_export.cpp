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
    std::string function_name_s = std::string((char *)function_name);
    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto error = "function " + function_name_s + " has been registered";
        Out::Log(pType::ERROR, error.c_str());
        return;
    }

    std::string _m = std::string("compiling function ") + function_name_s + ", ";

    std::vector<std::string> args_name_v;
    std::string _cache;
    while(*args_name) {
        char c = reinterpret_cast<char&>(*args_name);
        if(c == ',') {
            if(_cache.length()) {
                args_name_v.push_back(_cache);
                _cache = "";
            }
        } else {
            _cache += c;
        }
        args_name += 1;
    }

    _m += std::string("its prototype is") + (char)10;
    _m += "=====> ";
    _m += llvm_taichi::DataTypeStr((llvm_taichi::DataType)return_type);
    _m += " " + function_name_s + "(";
    for(uint8_t i = 0; i < args_number; i += 1) {
        if(i) {
            _m += ", ";
        }
        _m += llvm_taichi::DataTypeStr((llvm_taichi::DataType)args_type[i]);
        _m += " " + args_name_v[i];
    }
    _m += std::string(") <=====");
    Out::Log(pType::MESSAGE, _m.c_str());

    auto this_func = std::make_shared<llvm_taichi::Function>();
    llvm_taichi::taichi_func_table[function_name_s] = this_func;

    std::vector<llvm_taichi::Argument> args_v;
    for(uint8_t i = 0; i < args_number; i += 1) {
        args_v.push_back((llvm_taichi::Argument){
            (llvm_taichi::DataType)args_type[i],
            args_name_v[i]
        });
    }
    
    this_func->build_begin(
        function_name_s,
        args_v,
        (llvm_taichi::DataType)return_type
    );
}

void function_finish(
    uint8_t *function_name
) {
    std::string function_name_s = std::string((char *)function_name);
    std::string _m = "function " + function_name_s + " build complated";
    Out::Log(pType::MESSAGE, _m.c_str());

    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto this_func = llvm_taichi::taichi_func_table[function_name_s];
        this_func->build_finish();
    }
}