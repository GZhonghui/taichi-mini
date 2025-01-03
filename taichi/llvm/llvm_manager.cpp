#include "llvm_manager.h"

namespace llvm_taichi
{

void init()
{
    
}

void Function::build_begin(
    const std::string &function_name,
    const std::vector<Argument> &argument_list,
    DataType return_type
)
{
    this->name = function_name;
    this->argument_list.clear();
    for(auto arg : argument_list) {
        this->argument_list.push_back(arg);
    }
    this->return_type = return_type;
    this->variable_type_table.clear();

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
    Byte *result = new Byte[type_size(return_type)];

    return std::shared_ptr<Byte[]>(result, std::default_delete<Byte[]>());
}

}