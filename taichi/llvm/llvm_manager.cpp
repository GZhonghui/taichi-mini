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

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(*taichi_context, "entry", this->llvm_function);
    this->current_builder->SetInsertPoint(entry_block);
}

void Function::build_finish()
{

}

void Function::loop_begin(uint32_t l, uint32_t r, uint32_t s)
{

}

void Function::loop_finish()
{

}

void Function::assignment_statement(
    const std::string &result_name,
    const Value &left_value,
    OperationType operation_type,
    const Value &right_value
)
{

}

void Function::return_statement(const std::string &return_variable_name)
{

}

std::shared_ptr<Byte[]> Function::run(Byte *argument_buffer)
{
    Byte *result_buffer = new Byte[type_size(return_type)];

    std::vector<llvm::GenericValue> args;
    llvm::ArrayRef<llvm::GenericValue> args_array(args);

    if(this->llvm_function) {
        llvm::GenericValue result = taichi_engine->runFunction(
            this->llvm_function,
            args_array
        );

        uint64_t result_value = result.IntVal.getZExtValue();
        Byte *src_buffer = (Byte *)&result_value;
        memcpy(result_buffer, src_buffer, type_size(return_type)); // TODO
    }

    return std::shared_ptr<Byte[]>(result_buffer, std::default_delete<Byte[]>());
}

}