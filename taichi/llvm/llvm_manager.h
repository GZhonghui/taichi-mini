#ifndef LLVM_MANAGER_H
#define LLVM_MANAGER_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

namespace llvm_taichi
{
    typedef uint8_t Byte;

    class OperationValue;
    class Function;

    enum DataType {
        Int32 = 1,
        Int64 = 2,
        Float32 = 3,
        Float64 = 4
    };

    enum OperationType {
        Add = 1,
        Sub = 2,
        Mul = 3,
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

    inline uint8_t type_size(DataType type) {
        uint8_t res = 1;
        switch(type) {
            case DataType::Int32:
            case DataType::Float32:
                res = 4;
                break;
            case DataType::Int64:
            case DataType::Float64:
                res = 8;
                break;
        }
        return res;
    }

    inline llvm::Type *to_llvm_type(DataType type, llvm::LLVMContext *context) {
        llvm::Type *res = nullptr;
        switch (type) {
            case DataType::Int32:
                res = llvm::Type::getInt32Ty(*context);
                break;
            case DataType::Int64:
                res = llvm::Type::getInt64Ty(*context);
                break;
            case DataType::Float32:
                res = llvm::Type::getFloatTy(*context);
                break;
            case DataType::Float64:
                res = llvm::Type::getDoubleTy(*context);
                break;
        }
        return res;
    }

    inline llvm::Value *llvm_default_value(
        DataType type,
        llvm::LLVMContext *context
    ) {
        llvm::Value *res = nullptr;
        switch (type) {
            case DataType::Int32:
            case DataType::Int64:
                res = llvm::ConstantInt::get(to_llvm_type(type, context), 0);
                break;
            case DataType::Float32:
            case DataType::Float64:
                res = llvm::ConstantFP::get(to_llvm_type(type, context), 0.0);            
                break;
        }
        return res;
    }

    inline bool is_int(DataType type) {
        return type == DataType::Int32 || type == DataType::Int64;
    }

    inline bool is_float(DataType type) {
        return type == DataType::Float32 || type == DataType::Float64;
    }

    inline DataType calc_type(DataType a, DataType b) {
        uint8_t res_size = std::max(type_size(a), type_size(b));
        DataType res = DataType::Int32;
        if(res_size == 4) {
            res = (is_float(a) || is_float(b)) ? DataType::Float32 : DataType::Int32;
        } else if(res_size == 8) {
            res = (is_float(a) || is_float(b)) ? DataType::Float64 : DataType::Int64;
        }
        return res;
    }

    inline llvm::Value *cast(
        DataType from,
        DataType to,
        llvm::Value *value,
        llvm::IRBuilder<> *builder,
        llvm::LLVMContext *context
    ) {
        if(from == to) return value;
        llvm::Value *res = nullptr;
        llvm::Type *target_type = to_llvm_type(to, context);
        switch(to) {
            case DataType::Int32:
                if(from == DataType::Int64) {
                    res = builder->CreateTrunc(
                        value,
                        target_type
                    );
                } else if(is_float(from)) {
                    res = builder->CreateFPToSI(
                        value,
                        target_type
                    );
                }
                break;
            case DataType::Int64:
                if(from == DataType::Int32) {
                    res = builder->CreateSExt(
                        value,
                        target_type
                    );
                } else if(is_float(from)) {
                    res = builder->CreateFPToSI(
                        value,
                        target_type
                    );
                }
                break;
            case DataType::Float32:
                if(from == DataType::Float64) {
                    res = builder->CreateFPTrunc(
                        value,
                        target_type
                    );
                } else if(is_int(from)) {
                    res = builder->CreateSIToFP(
                        value,
                        target_type
                    );
                }
                break;
            case DataType::Float64:
                if(from == DataType::Float32) {
                    res = builder->CreateFPExt(
                        value,
                        target_type
                    );
                } else if(is_int(from)) {
                    res = builder->CreateSIToFP(
                        value,
                        target_type
                    );
                }
                break;
        }
        return res;
    }

    class OperationValue {
        friend class Function;

    protected:
        OperationValueType operation_value_type;
        std::string variable_name;
        DataType constant_value_type;
        Byte constant_value[8];
    
    public:
        DataType get_data_type(
            Function *function
        ) const;

        llvm::Value *construct_llvm_value(
            Function *function,
            llvm::IRBuilder<> *builder,
            llvm::LLVMContext *context
        ) const;
    };

    void init();

    class Function {
        friend class OperationValue;

    protected:
        std::string name;
        std::vector<Argument> argument_list;
        DataType return_type;
        llvm::Function *llvm_function;
        std::vector< 
            std::unordered_map<
                std::string,
                std::pair<llvm::AllocaInst *, DataType>
            >
        > variable_stack;
        std::unique_ptr<llvm::Module> current_module;
        std::unique_ptr< llvm::IRBuilder<> > current_builder;

    protected:
        std::pair<llvm::AllocaInst *, DataType> find_variable(const std::string &variable_name);
        llvm::AllocaInst *alloc_variable(const std::string &name, DataType type);

    public:
        void build_begin(
            const std::string &function_name,
            const std::vector<Argument> &argument_list,
            DataType return_type
        );
        void build_finish();
        void loop_begin(
            const std::string &loop_index_name,
            uint32_t l,
            uint32_t r,
            uint32_t s
        );
        void loop_finish();
        void assignment_statement(
            const std::string &name,
            const OperationValue &value
        );
        void assignment_statement(
            const std::string &result_name,
            const OperationValue &left_value,
            OperationType operation_type,
            const OperationValue &right_value
        );
        void return_statement(const std::string &return_variable_name);
        std::shared_ptr<Byte[]> run(Byte *argument_buffer);

    public:
        Function() = default;
    };

    extern std::unordered_map< std::string, std::shared_ptr<Function> > taichi_func_table;
    extern std::unique_ptr<llvm::ExecutionEngine> taichi_engine;
    extern std::unique_ptr<llvm::LLVMContext> taichi_context;
}

#endif