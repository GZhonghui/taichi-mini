#ifndef LLVM_MANAGER_H
#define LLVM_MANAGER_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

namespace llvm_taichi
{
    typedef uint8_t Byte;

    enum DataType {
        Int32 = 1,
        Int64 = 2,
        Float32 = 3,
        Float64 = 4
    };

    enum OperationType {
        Add = 1,
        Sub = 2,
        Mult = 3,
        Div = 4
    };

    enum OperationValueType {
        Constant = 1,
        Variable = 2
    };

    struct Argument {
        DataType type;
        std::string name;
    };

    struct Value {
        OperationValueType type;
        std::string variable_name;
        Byte constant_value[8];
    };

    inline uint8_t type_size(DataType type) {
        switch(type) {
            case DataType::Int32:
            case DataType::Float32:
                return 4;
            case DataType::Int64:
            case DataType::Float64:
                return 8;
        }
        return 1;
    }

    void init();

    class Function {
    protected:
        std::string name;
        std::vector<Argument> argument_list;
        DataType return_type;
        std::unordered_map<std::string, DataType> variable_type_table;

    public:
        void build_begin(
            const std::string &function_name,
            const std::vector<Argument> &argument_list,
            DataType return_type
        );
        void build_finish();
        void loop_begin(uint32_t l, uint32_t r, uint32_t s);
        void loop_finish();
        void assignment_statement(
            const std::string &result_name,
            const Value &left_value,
            OperationType operation_type,
            const Value &right_value
        );
        void return_statement(const std::string &return_variable_name);
        std::shared_ptr<Byte[]> run(Byte *argument_buffer);

    public:
        Function() = default;
    };

    extern std::unordered_map< std::string, std::shared_ptr<Function> > taichi_func_table;
}

#endif