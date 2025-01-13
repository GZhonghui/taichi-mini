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

// lib 的 namespace
namespace llvm_taichi
{
    // 喜欢这样表示字节
    typedef uint8_t Byte;

    // Class 需要相互引用的话，可以提前声明
    class OperationValue;
    class Function;

    // 数据类型
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

    // 运算类型
    enum OperationType {
        Add = 1,
        Sub = 2,
        Mul = 3,
        Div = 4
    };

    // 操作数类型：常量 or 变量
    enum OperationValueType {
        Constant = 1,
        Variable = 2
    };

    // 通用参数
    struct Argument {
        DataType type;
        std::string name;
    };

    // 获取一个类型的字节数量
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

    // Taichi 类型转换为 LLVM 类型
    inline llvm::Type *to_llvm_type(DataType type, llvm::LLVMContext *context) {
        llvm::Type *res = nullptr;
        switch (type) {
            case DataType::Int32:
                res = llvm::Type::getInt32Ty(*context); // 获取的是类型
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

    // 构造一个 LLVM 某类型的默认常量
    inline llvm::Value *llvm_default_value(
        DataType type,
        llvm::LLVMContext *context
    ) {
        llvm::Value *res = nullptr;
        switch (type) {
            case DataType::Int32:
            case DataType::Int64:
                res = llvm::ConstantInt::get(to_llvm_type(type, context), 0, true); // 获取的是一个数值
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

    // 计算类型提升：a 和 b 计算的结果应该是什么类型
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
        DataType from, // 原始类型
        DataType to, // 目标类型
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
                    res = builder->CreateFPTrunc( // 浮点数截断
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

    // 操作数（出现在表达式 or 赋值语句中）
    class OperationValue {
        friend class Function; // 哈哈，友元

    protected:
        OperationValueType operation_value_type; // 常量 or 变量
        std::string variable_name; // 变量的话，存储变量名
        DataType constant_value_type; // 常量的话，需要数据类型
        Byte constant_value[8]; // 存储常量的值

    public:
        // 设定变量
        inline void set_variable(const std::string &name) {
            operation_value_type = OperationValueType::Variable;
            variable_name = name;
        }

        // 设定常量
        inline void set_constant(DataType type, Byte *source_buffer) {
            operation_value_type = OperationValueType::Constant;
            constant_value_type = type;
            memcpy(constant_value, source_buffer, type_size(type));
        }

        // 从一个 buffer 解析数据
        inline void from_buffer(Byte *buffer) {
            if(*buffer) { // buffer 的第一个字节表示是不是常量
                set_constant(
                    (DataType)buffer[1],
                    buffer + 2
                );
            } else {
                set_variable(std::string((char *)(buffer + 2)));
            }
        }
    
    public:
        // 获取数据类型
        DataType get_data_type(
            Function *function
        ) const;

        // 根据自己的数据，构造一个 LLVM Value
        llvm::Value *construct_llvm_value(
            Function *function,
            llvm::IRBuilder<> *builder,
            llvm::LLVMContext *context
        ) const;
    };

    void init(); // 初始化 lib

    // 函数
    class Function {
        friend class OperationValue;

    protected:
        std::string name; // 函数名
        std::vector<Argument> argument_list; // 参数列表
        DataType return_type; // 返回值类型
        llvm::Function *llvm_function; // LLVM Func 指针
        std::vector< 
            std::unordered_map<
                std::string,
                std::pair<llvm::AllocaInst *, DataType>
            >
        > variable_stack;
        // 用栈（这里用 vector 模仿栈，方便遍历）存储函数内部的变量，比如进入 loop，就是进入一个新的作用域，内部的变量可以覆盖外部的变量
        // 每一层（可以理解为一个作用域）都是一个表，存储「变量名」 to 「变量的信息」
        // 这个「栈」记录了变量指针（非原始指针）和变量类型
        std::unique_ptr<llvm::Module> current_module; // 当前的 module，一个函数对应一个 module
        std::unique_ptr< llvm::IRBuilder<> > current_builder; // 当前的 builder，也是一个函数对应一个 builder
        // blocks，用栈存储，也是用于维护函数内部的作用域的
        std::stack<llvm::BasicBlock *> current_blocks;
        // 存储 loop 的状态，比如 loop index 的指针，用于更新 loop 状态
        std::stack< std::pair<llvm::AllocaInst *, int32_t> > current_loop_update;

    protected:
        // 从 stack 中找到一个变量，最先找到的就是最「内部」的变量，以此做到「内部变量掩盖外部变量」
        std::pair<llvm::AllocaInst *, DataType> find_variable(const std::string &variable_name);
        // 分配一个变量，也是分配到栈的顶部
        llvm::AllocaInst *alloc_variable(const std::string &name, DataType type, bool force_local = false);

    public:
        // 获取 llvm::Function
        // 层级： taichi::Function > llvm::Function > raw_func_ptr
        inline llvm::Function *get_raw_ptr() {
            return llvm_function;
        }

    // 用于定义函数的一系列接口
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

    // 定义 LLVM 所需要的「全局状态」，一般来说全局只需要一个 LLVMUnit
    // 为什么单独定义一个 Class 呢？
    // 因为希望在析构函数中控制资源的释放时机
    class LLVMUnit {
    public:
        // 目前需要这两个：执行引擎 & 上下文
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

    // 函数的注册表
    extern std::unordered_map< std::string, std::shared_ptr<Function> > taichi_func_table;
    // LLVM 的全局状态
    extern std::unique_ptr<LLVMUnit> taichi_llvm_unit;
}

#endif