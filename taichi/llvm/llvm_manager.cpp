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

std::pair<llvm::AllocaInst *, DataType> Function::find_variable(const std::string &variable_name)
{
    if(variable_type_table.count(variable_name)) {
        return std::make_pair(variable_table[variable_name], variable_type_table[variable_name]);
    }

    return std::make_pair<llvm::AllocaInst *, DataType>(nullptr, DataType::Int32);
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
    this->variable_type_table.clear();
    for(auto arg : argument_list) {
        this->argument_list.push_back(arg);
        this->variable_type_table[arg.name] = arg.type;
    }

    this->current_module = std::make_unique<llvm::Module>(
        "taichi_module_" + function_name,
        *taichi_context
    );
    this->current_builder = std::make_unique< llvm::IRBuilder<> >(
        *taichi_context
    );

    llvm::Type *llvm_return_type = to_llvm_type(return_type, taichi_context.get());
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

    this->variable_table.clear();
    llvm::Function::arg_iterator arg_begin = this->llvm_function->arg_begin();
    for(auto arg : this->argument_list) {
        this->variable_table[arg.name] = current_builder->CreateAlloca(
            to_llvm_type(arg.type, taichi_context.get()),
            nullptr
        );
        current_builder->CreateStore(arg_begin, this->variable_table[arg.name]);
        arg_begin ++;
    }
}

void Function::build_finish()
{

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
    if(!variable_type_table.count(name)) {
        variable_type_table[name] = value.get_data_type(variable_type_table);
        variable_table[name] = current_builder->CreateAlloca(
            to_llvm_type(variable_type_table[name], taichi_context.get()),
            nullptr
        );
    }

    current_builder->CreateStore(
        cast(
            value.get_data_type(variable_type_table),
            variable_type_table[name],
            value.construct_llvm_value(
                variable_type_table,
                variable_table,
                current_builder.get(),
                taichi_context.get()
            ),
            current_builder.get(),
            taichi_context.get()
        ),
        variable_table[name]
    );
}

void Function::assignment_statement(
    const std::string &result_name,
    const OperationValue &left_value,
    OperationType operation_type,
    const OperationValue &right_value
)
{
    if(!variable_type_table.count(result_name)) {
        DataType left_type = left_value.get_data_type(variable_type_table);
        DataType right_type = right_value.get_data_type(variable_type_table);
        DataType result_type = calc_type(left_type, right_type);

        variable_type_table[result_name] = result_type;
        variable_table[result_name] = current_builder->CreateAlloca(
            to_llvm_type(result_type, taichi_context.get()),
            nullptr
        );
    }

    DataType result_type = variable_type_table[result_name];
    llvm::Value *llvm_left_value = cast(
        left_value.get_data_type(variable_type_table),
        result_type,
        left_value.construct_llvm_value(
            variable_type_table,
            variable_table,
            current_builder.get(),
            taichi_context.get()
        ),
        current_builder.get(),
        taichi_context.get()
    );

    llvm::Value *llvm_right_value = cast(
        right_value.get_data_type(variable_type_table),
        result_type,
        right_value.construct_llvm_value(
            variable_type_table,
            variable_table,
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
        current_builder->CreateStore(llvm_result, variable_table[result_name]);
    }
}

void Function::return_statement(const std::string &return_variable_name)
{
    if(variable_type_table.count(return_variable_name)) {
        llvm::Value *load_value = current_builder->CreateLoad(
            to_llvm_type(
                variable_type_table[return_variable_name],
                taichi_context.get()
            ),
            variable_table[return_variable_name]
        );
        llvm::Value *cast_value = cast(
            variable_type_table[return_variable_name],
            return_type,
            load_value,
            current_builder.get(),
            taichi_context.get()
        );
        current_builder->CreateRet(cast_value);
    } else {
        current_builder->CreateRet(llvm_default_value(
            return_type,
            current_builder.get(),
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