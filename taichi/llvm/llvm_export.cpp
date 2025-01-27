#define TOOL_PRINT_H_DATA
#include "llvm_export.h"

void init_lib() {
    llvm_taichi::init();
}

extern "C" void set_log_level(uint8_t level) {
    Out::logLevel = (pType)level;
}

void function_begin(
    uint8_t *function_name,
    uint8_t args_number,
    uint8_t *args_type,
    uint8_t *args_name,
    uint8_t return_type
) {
    // 所有的函数都是注册到 llvm_taichi::taichi_func_table 里面的
    std::string function_name_s = std::string((char *)function_name);
    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto error = "function " + function_name_s + " has been registered";
        Out::Log(pType::ERROR, error.c_str());
        return;
    }

    std::string _m = std::string("compiling function ") + function_name_s + ", ";

    std::vector<std::string> args_name_v;
    std::string _cache;
    while(*args_name) { // args_name 中分割出参数名称（使用逗号分割）
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

    _m += std::string("its prototype is") + (char)10; // 喜欢这种换行的写法 没有反斜杠
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
    Out::Log(pType::DEBUG, _m.c_str());

    // 注册新函数
    auto this_func = std::make_shared<llvm_taichi::Function>();
    llvm_taichi::taichi_func_table[function_name_s] = this_func;

    std::vector<llvm_taichi::Argument> args_v;
    for(uint8_t i = 0; i < args_number; i += 1) {
        args_v.push_back((llvm_taichi::Argument){
            (llvm_taichi::DataType)args_type[i],
            args_name_v[i]
        });
    }
    
    // 开始函数定义
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
    Out::Log(pType::DEBUG, _m.c_str());

    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto this_func = llvm_taichi::taichi_func_table[function_name_s];
        this_func->build_finish(); // 结束函数定义
    }
}

extern "C" void loop_begin(
    uint8_t *function_name,
    uint8_t *loop_index_name,
    int32_t l,
    int32_t r,
    int32_t s
) {
    std::string function_name_s = std::string((char *)function_name);
    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto this_func = llvm_taichi::taichi_func_table[function_name_s];
        this_func->loop_begin( // 开始循环定义
            std::string((char *)loop_index_name),
            l,
            r,
            s
        );
    }
}

extern "C" void loop_finish(
    uint8_t *function_name
) {
    std::string function_name_s = std::string((char *)function_name);
    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto this_func = llvm_taichi::taichi_func_table[function_name_s];
        this_func->loop_finish(); // 结束循环
    }
}

void assignment_statement_value(
    uint8_t *function_name,
    uint8_t *target_variable_name,
    uint8_t *source_buffer
) {
    std::string function_name_s = std::string((char *)function_name);
    if(!llvm_taichi::taichi_func_table.count(function_name_s)) {
        return;
    }

    auto this_func = llvm_taichi::taichi_func_table[function_name_s];

    llvm_taichi::OperationValue value;
    value.from_buffer(source_buffer); // 直接从 buffer 解析得到一个 Value
    // 定义赋值语句
    this_func->assignment_statement(
        std::string((char *)target_variable_name),
        value
    );
}

void assignment_statement_operation(
    uint8_t *function_name,
    uint8_t *target_variable_name,
    uint8_t *left_buffer,
    uint8_t operation_type,
    uint8_t *right_buffer
) {
    std::string function_name_s = std::string((char *)function_name);
    if(!llvm_taichi::taichi_func_table.count(function_name_s)) {
        return;
    }

    auto this_func = llvm_taichi::taichi_func_table[function_name_s];

    llvm_taichi::OperationValue left, right;
    left.from_buffer(left_buffer);
    right.from_buffer(right_buffer);
    // 定义赋值语句
    this_func->assignment_statement(
        std::string((char *)target_variable_name),
        left,
        (llvm_taichi::OperationType)operation_type,
        right
    );
}

void return_statement(
    uint8_t *function_name,
    uint8_t *return_variable_name
) {
    std::string function_name_s = std::string((char *)function_name);
    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto this_func = llvm_taichi::taichi_func_table[function_name_s];
        // 定义 return
        this_func->return_statement(std::string((char *)return_variable_name));
    }
}

void run(
    uint8_t *function_name,
    uint8_t *argument_buffer,
    uint8_t *result_buffer
) {
    std::string function_name_s = std::string((char *)function_name);
    if(!llvm_taichi::taichi_func_table.count(function_name_s)) {
        return;
    }

    auto this_func = llvm_taichi::taichi_func_table[function_name_s];
    this_func->run(argument_buffer, result_buffer); // 在 C 端调用函数，实际上这种方式并不可行
}

void *get_func_ptr(
    uint8_t *function_name
) {
    std::string function_name_s = std::string((char *)function_name);

    if(llvm_taichi::taichi_func_table.count(function_name_s)) {
        auto this_func = llvm_taichi::taichi_func_table[function_name_s];
        auto llvm_func_ptr = this_func->get_raw_ptr();
        if(llvm_func_ptr) {
            // 注意要得到原始指针
            return llvm_taichi::taichi_llvm_unit->engine->getPointerToFunction(llvm_func_ptr);
        }
    }

    auto llvm_func_ptr = llvm_taichi::taichi_llvm_unit->engine->FindFunctionNamed(function_name_s);
    if(llvm_func_ptr) {
        return llvm_taichi::taichi_llvm_unit->engine->getPointerToFunction(llvm_func_ptr);
    }

    std::string _m = "can not find address of function " + function_name_s;
    Out::Log(pType::ERROR, _m.c_str());
    return nullptr;
}