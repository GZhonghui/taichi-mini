#ifndef LLVM_MANAGER_H
#define LLVM_MANAGER_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <unordered_map>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include "../tool/print.h"

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

    // 从枚举类型转换为字符串 可以参照这种写法
    // switch 是跳表 执行很快
    // 字符串也都是常量类型
    inline const char *DataTypeStr(DataType type) {
        switch(type) {
            case DataType::Int32:
                return "Int32";
            case DataType::Int64:
                return "Int64";
            case DataType::Float32:
                return "Float32";
            case DataType::Float64:
                return "Float64";
            default:
                return "taichi_default_data_type";
        }
    }

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
                res = llvm::ConstantInt::get(to_llvm_type(type, context), 0, true);
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

    // 类型转换
    // 每种情况单独处理（因为逻辑确实不同）
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
                    res = builder->CreateTrunc( // 截断
                        value,
                        target_type
                    );
                } else if(is_float(from)) {
                    res = builder->CreateFPToSI( // FP: 浮点数 SI: Signed Int
                        value,
                        target_type
                    );
                }
                break;
            case DataType::Int64:
                if(from == DataType::Int32) {
                    res = builder->CreateSExt( // 拓展
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
                    res = builder->CreateFPExt( // 浮点数拓展
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
        inline void set_variable(const std::string &name) {
            operation_value_type = OperationValueType::Variable;
            variable_name = name;
        }

        inline void set_constant(DataType type, Byte *source_buffer) {
            operation_value_type = OperationValueType::Constant;
            constant_value_type = type;
            memcpy(constant_value, source_buffer, type_size(type));
        }

        inline void from_buffer(Byte *buffer) {
            if(*buffer) {
                set_constant(
                    (DataType)buffer[1],
                    buffer + 2
                );
            } else {
                set_variable(std::string((char *)(buffer + 2)));
            }
        }
    
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
        std::stack<llvm::BasicBlock *> current_blocks;
        std::stack< std::pair<llvm::AllocaInst *, int32_t> > current_loop_update;

    protected:
        std::pair<llvm::AllocaInst *, DataType> find_variable(const std::string &variable_name);
        llvm::AllocaInst *alloc_variable(const std::string &name, DataType type, bool force_local = false);

    public:
        inline llvm::Function *get_raw_ptr() {
            return llvm_function;
        }

    public:
        void build_begin(
            const std::string &function_name,
            const std::vector<Argument> &argument_list,
            DataType return_type
        );
        void build_finish();
        void loop_begin(
            const std::string &loop_index_name,
            int32_t l,
            int32_t r,
            int32_t s
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
        std::shared_ptr<Byte[]> run(Byte *argument_buffer, Byte *result_buffer);

    public:
        Function() = default;
    };

    class LLVMUnit {
    public:
        llvm::ExecutionEngine *engine;
        llvm::LLVMContext *context;

    public:
        inline LLVMUnit() {
            engine = nullptr;
            context = nullptr;
        }

        inline ~LLVMUnit() {
            Out::Log(pType::DEBUG, "ready to detroy llvm unit");
            // AI: llvm::ExecutionEngine 内部依赖于 llvm::LLVMContext，因此 LLVMContext 的生命周期必须比 ExecutionEngine 长
            if(engine) { // must destroy before context
                // delete engine;
                // engine = nullptr;
                // 在这里销毁 engine 会出错
                // TODO: engine 并不是显式通过 new 创建的，确认这个 engine 真的需要手动销毁吗？
                // 如果需要销毁的话，engine 需要首先销毁（先于 context）
            }
            if(context) {
                delete context; // context 是显式手动创建的
                context = nullptr;
            }
        }
    };

    extern std::unordered_map< std::string, std::shared_ptr<Function> > taichi_func_table;
    extern std::unique_ptr<LLVMUnit> taichi_llvm_unit;
}

#endif