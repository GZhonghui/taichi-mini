#include "llvm_manager.h"

namespace llvm_taichi
{

std::unordered_map< std::string, std::shared_ptr<Function> > taichi_func_table;
std::unique_ptr<llvm::ExecutionEngine> taichi_engine;
std::unique_ptr<llvm::LLVMContext> taichi_context;

void init()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    taichi_context = std::make_unique<llvm::LLVMContext>();
    auto init_module = std::make_unique<llvm::Module>("TaichiInitModule", *taichi_context);

    std::string Error;
    llvm::ExecutionEngine *Engine = llvm::EngineBuilder(std::move(init_module))
        .setErrorStr(&Error)
        .setOptLevel(llvm::CodeGenOptLevel::Aggressive)
        .create();

    taichi_engine = std::unique_ptr<llvm::ExecutionEngine>(Engine);
}

DataType OperationValue::get_data_type(
    Function *function
) const
{
    DataType res = DataType::Int32;
    if(operation_value_type == OperationValueType::Constant) {
        res = constant_value_type;
    } else if(operation_value_type == OperationValueType::Variable) {
        auto find_res = function->find_variable(variable_name);
        if(find_res.first) {
            res = find_res.second;
        }
    }
    return res;
}

llvm::Value *OperationValue::construct_llvm_value(
    Function *function,
    llvm::IRBuilder<> *builder,
    llvm::LLVMContext *context
) const
{
    llvm::Value *res = nullptr;
    if(operation_value_type == OperationValueType::Constant) {
        uint32_t cache_32 = 0;
        uint64_t cache_64 = 0;
        switch(constant_value_type) {
            case DataType::Int32:
                memcpy(&cache_32, constant_value, 4);
                res = llvm::ConstantInt::get(
                    to_llvm_type(constant_value_type, context),
                    cache_32
                );
                break;
            case DataType::Int64:
                memcpy(&cache_64, constant_value, 8);
                res = llvm::ConstantInt::get(
                    to_llvm_type(constant_value_type, context),
                    cache_64
                );
                break;
            case DataType::Float32:
                memcpy(&cache_32, constant_value, 4);
                res = llvm::ConstantFP::get(
                    to_llvm_type(constant_value_type, context),
                    static_cast<double>(
                        reinterpret_cast<float&>(cache_32)
                    )
                );
                break;
            case DataType::Float64:
                memcpy(&cache_64, constant_value, 8);
                res = llvm::ConstantFP::get(
                    to_llvm_type(constant_value_type, context),
                    reinterpret_cast<double&>(cache_64)
                );
                break;
        }
    } else if(operation_value_type == OperationValueType::Variable) {
        auto find_result = function->find_variable(variable_name);
        if(find_result.first) {
            res = builder->CreateLoad(
                to_llvm_type(find_result.second, context),
                find_result.first
            );
        }
    }
    return res;
}

std::pair<llvm::AllocaInst *, DataType> Function::find_variable(const std::string &variable_name)
{
    for(int32_t i = variable_stack.size() - 1; i >= 0; i -= 1) {
        auto env = &(variable_stack[i]);
        if(env->count(variable_name)) {
            return env->at(variable_name);
        }
    }
    
    return std::make_pair<llvm::AllocaInst *, DataType>(nullptr, DataType::Int32);
}

llvm::AllocaInst *Function::alloc_variable(const std::string &name, DataType type)
{
    auto res = find_variable(name);
    if(!res.first) {
        llvm::AllocaInst *ptr = current_builder->CreateAlloca(
            to_llvm_type(type, taichi_context.get())
        );
        variable_stack.back()[name] = std::make_pair(
            ptr,
            type
        );
        return ptr;
    }
    return res.first;
}

void Function::build_begin(
    const std::string &function_name,
    const std::vector<Argument> &argument_list,
    DataType return_type
)
{
    this->name = function_name;
    this->return_type = return_type;
    
    this->argument_list.clear();
    this->variable_stack.clear();
    for(auto arg : argument_list) {
        this->argument_list.push_back(arg);
    }

    this->current_module = std::make_unique<llvm::Module>(
        "taichi_module_" + function_name,
        *taichi_context
    );
    this->current_builder = std::make_unique< llvm::IRBuilder<> >(
        *taichi_context
    );

    llvm::Type *llvm_return_type = to_llvm_type(
        this->return_type,
        taichi_context.get()
    );
    std::vector<llvm::Type *> llvm_args_type;
    for(auto arg : this->argument_list) {
        llvm_args_type.push_back(to_llvm_type(arg.type, taichi_context.get()));
    }
    llvm::ArrayRef<llvm::Type *> llvm_args_type_array(llvm_args_type);

    llvm::FunctionType *func_type = llvm::FunctionType::get(
        llvm_return_type,
        llvm_args_type_array,
        false
    );

    this->llvm_function = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        this->name,
        *(this->current_module)
    );

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(
        *taichi_context,
        "function_entry",
        this->llvm_function
    );
    this->current_builder->SetInsertPoint(entry_block);

    this->variable_stack.push_back(
        std::unordered_map<
            std::string,
            std::pair<llvm::AllocaInst *, DataType>
        >()
    );
    llvm::Function::arg_iterator arg_begin = this->llvm_function->arg_begin();
    for(auto arg : this->argument_list) {
        auto ptr = alloc_variable(arg.name, arg.type);
        current_builder->CreateStore(arg_begin++, ptr);
    }
}

void Function::build_finish()
{
    taichi_engine->addModule(std::move(current_module));
}

void Function::loop_begin(
    const std::string &loop_index_name,
    uint32_t l,
    uint32_t r,
    uint32_t s
)
{

}

void Function::loop_finish()
{

}

void Function::assignment_statement(
    const std::string &name,
    const OperationValue &value
)
{
    if(name == value.variable_name) return;

    auto target_find_result = find_variable(name);
    if(!target_find_result.first) {
        alloc_variable(name, value.get_data_type(this));
    }

    target_find_result = find_variable(name);
    current_builder->CreateStore(
        cast(
            value.get_data_type(this),
            target_find_result.second,
            value.construct_llvm_value(
                this,
                current_builder.get(),
                taichi_context.get()
            ),
            current_builder.get(),
            taichi_context.get()
        ),
        target_find_result.first
    );
}

void Function::assignment_statement(
    const std::string &result_name,
    const OperationValue &left_value,
    OperationType operation_type,
    const OperationValue &right_value
)
{
    auto find_result = find_variable(result_name);
    if(!find_result.first) {
        DataType left_type = left_value.get_data_type(this);
        DataType right_type = right_value.get_data_type(this);
        DataType result_type = calc_type(left_type, right_type);

        alloc_variable(result_name, result_type);
    }

    find_result = find_variable(result_name);
    DataType result_type = find_result.second;

    llvm::Value *llvm_left_value = cast(
        left_value.get_data_type(this),
        result_type,
        left_value.construct_llvm_value(
            this,
            current_builder.get(),
            taichi_context.get()
        ),
        current_builder.get(),
        taichi_context.get()
    );

    llvm::Value *llvm_right_value = cast(
        right_value.get_data_type(this),
        result_type,
        right_value.construct_llvm_value(
            this,
            current_builder.get(),
            taichi_context.get()
        ),
        current_builder.get(),
        taichi_context.get()
    );

    llvm::Value *llvm_result = nullptr;
    switch(operation_type) {
        case OperationType::Add:
            if(is_int(result_type)) {
                llvm_result = current_builder->CreateAdd(
                    llvm_left_value,
                    llvm_right_value
                );
            } else if(is_float(result_type)) {
                llvm_result = current_builder->CreateFAdd(
                    llvm_left_value,
                    llvm_right_value
                );
            }
            break;
        case OperationType::Sub:
            if(is_int(result_type)) {
                llvm_result = current_builder->CreateSub(
                    llvm_left_value,
                    llvm_right_value
                );
            } else if(is_float(result_type)) {
                llvm_result = current_builder->CreateFSub(
                    llvm_left_value,
                    llvm_right_value
                );
            }
            break;
        case OperationType::Mul:
            if(is_int(result_type)) {
                llvm_result = current_builder->CreateMul(
                    llvm_left_value,
                    llvm_right_value
                );
            } else if(is_float(result_type)) {
                llvm_result = current_builder->CreateFMul(
                    llvm_left_value,
                    llvm_right_value
                );
            }
            break;
        case OperationType::Div:
            if(is_int(result_type)) {
                llvm_result = current_builder->CreateSDiv(
                    llvm_left_value,
                    llvm_right_value
                );
            } else if(is_float(result_type)) {
                llvm_result = current_builder->CreateFDiv(
                    llvm_left_value,
                    llvm_right_value
                );
            }
            break;
    }
    if(llvm_result) {
        current_builder->CreateStore(llvm_result, find_result.first);
    }
}

void Function::return_statement(const std::string &return_variable_name)
{
    auto find_result = find_variable(return_variable_name);
    if(find_result.first) {
        llvm::Value *load_value = current_builder->CreateLoad(
            to_llvm_type(
                find_result.second,
                taichi_context.get()
            ),
            find_result.first
        );
        llvm::Value *cast_value = cast(
            find_result.second,
            return_type,
            load_value,
            current_builder.get(),
            taichi_context.get()
        );
        current_builder->CreateRet(cast_value);
    } else {
        current_builder->CreateRet(llvm_default_value(
            return_type,
            taichi_context.get()
        ));
    }
}

std::shared_ptr<Byte[]> Function::run(Byte *argument_buffer)
{
    Byte *result_buffer = new Byte[type_size(return_type)];

    // TODO
    std::vector<llvm::GenericValue> args;
    llvm::ArrayRef<llvm::GenericValue> args_array(args);

    if(this->llvm_function) {
        llvm::GenericValue result = taichi_engine->runFunction(
            this->llvm_function,
            args_array
        );

        uint64_t result_value = result.IntVal.getZExtValue();
        Byte *src_buffer = (Byte *)&result_value;
        memcpy(result_buffer, src_buffer, type_size(return_type)); // CHECK
    }

    return std::shared_ptr<Byte[]>(result_buffer, std::default_delete<Byte[]>());
}

}