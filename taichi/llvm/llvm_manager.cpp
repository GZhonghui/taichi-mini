#include "llvm_manager.h"

namespace llvm_taichi
{

// 需要「正式」声明分配空间，只有头文件的 extern 不够
std::unordered_map< std::string, std::shared_ptr<Function> > taichi_func_table;
std::unique_ptr<LLVMUnit> taichi_llvm_unit;

void init()
{
    Out::Log(pType::DEBUG, "initing llvm lib...");
    taichi_llvm_unit = std::make_unique<LLVMUnit>(); // 创建 LLVM 的全局状态

    // 初始化
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // 创建全局上下文
    taichi_llvm_unit->context = new llvm::LLVMContext();
    // 感觉这个 InitModule 可有可无
    auto init_module = std::make_unique<llvm::Module>("TaichiInitModule", *(taichi_llvm_unit->context));

    // 构建「执行引擎」
    std::string Error;
    llvm::ExecutionEngine *Engine = llvm::EngineBuilder(std::move(init_module)) // 转交所有权
        .setErrorStr(&Error)
        .setOptLevel(llvm::CodeGenOptLevel::None)
        .create();
    if(!Engine || Error.length()) {
        Out::Log(pType::ERROR, Error.c_str());
    }
    
    taichi_llvm_unit->engine = Engine;
    taichi_llvm_unit->engine->finalizeObject();
    Out::Log(pType::DEBUG, "llvm lib init complated");

    // DEBUG
    // 手动创建一个函数（而不是使用 Python 调用的接口），用于验证功能
    {
        auto Module = std::make_unique<llvm::Module>("debug_module", *(taichi_llvm_unit->context));
        llvm::IRBuilder<> Builder(*(taichi_llvm_unit->context));

        llvm::Type *Int32Type = llvm::Type::getInt32Ty(*(taichi_llvm_unit->context));
        llvm::FunctionType *FuncType = llvm::FunctionType::get(
            Int32Type,
            {Int32Type, Int32Type},
            false
        );
        llvm::Function *AddFunc = llvm::Function::Create(
            FuncType,
            llvm::Function::ExternalLinkage,
            "debug_add",
            *Module
        );

        llvm::BasicBlock *BB = llvm::BasicBlock::Create(
            *(taichi_llvm_unit->context),
            "entry",
            AddFunc
        );
        Builder.SetInsertPoint(BB);
        llvm::Argument *Arg1 = AddFunc->getArg(0);
        llvm::Argument *Arg2 = AddFunc->getArg(1);
        llvm::Value *Sum = Builder.CreateAdd(Arg1, Arg2, "sum");
        Builder.CreateRet(Sum);

        taichi_llvm_unit->engine->addModule(std::move(Module));
        taichi_llvm_unit->engine->finalizeObject();
        Out::Log(pType::DEBUG, "debug_add attached");
    }
    // DEBUG
}

DataType OperationValue::get_data_type(
    Function *function
) const
{
    DataType res = DataType::Int32;
    if(operation_value_type == OperationValueType::Constant) {
        res = constant_value_type; // 常量的话就直接返回记录的类型
    } else if(operation_value_type == OperationValueType::Variable) {
        auto find_res = function->find_variable(variable_name);
        if(find_res.first) {
            res = find_res.second; // 变量的话，就找到这个变量再返回其类型
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
    // 构造常量
    if(operation_value_type == OperationValueType::Constant) {
        uint32_t cache_32 = 0;
        uint64_t cache_64 = 0;
        switch(constant_value_type) {
            case DataType::Int32:
                memcpy(&cache_32, constant_value, 4);
                // 构造一个 int 常量
                // 其实还有别的方式
                // 1 首先构造一个 llvm::APInt 然后使用 llvm::ConstantInt::get
                // 2 Builder.getInt32
                // 这边的数据类型转换、截断规则还要再确认（构造32位的数据，传入的却是64位的数据）
                res = llvm::ConstantInt::get(
                    to_llvm_type(constant_value_type, context),
                    static_cast<uint64_t>(cache_32),
                    true
                );
                break;
            case DataType::Int64:
                memcpy(&cache_64, constant_value, 8);
                res = llvm::ConstantInt::get(
                    to_llvm_type(constant_value_type, context),
                    cache_64,
                    true
                );
                break;
            // 创建浮点数常量
            case DataType::Float32:
                memcpy(&cache_32, constant_value, 4);
                res = llvm::ConstantFP::get(
                    to_llvm_type(constant_value_type, context),
                    static_cast<double>(
                        reinterpret_cast<float&>(cache_32) // 这里强制转换
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
    // 变量的话，直接找到其地址，然后创建一个 Load 指令就可以了
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
    // 从栈顶开始找，实现作用域覆盖
    // 注意：实际上 Python 无法做到显示声明变量，Python 在作用域内部访问一个外部已有的变量，会被视为「访问」而不是「创建」
    // 所以这个覆盖的机制，一般只可能在 loop 的作用域内体现（loop 的 index 是强制覆盖的）
    for(int32_t i = variable_stack.size() - 1; i >= 0; i -= 1) {
        auto env = &(variable_stack[i]);
        if(env->count(variable_name)) {
            return env->at(variable_name);
        }
    }
    
    return std::make_pair<llvm::AllocaInst *, DataType>(nullptr, DataType::Int32); // 地址返回 nullptr 的话表示没有找到这个变量
}

// llvm::AllocaInst 是 llvm::Value 的子类
// 想取得 AllocaInst 的数据的话需要 Load
// AllocaInst 表示的是一个内存地址（即指针）
llvm::AllocaInst *Function::alloc_variable(const std::string &name, DataType type, bool force_local)
{
    // 强制创建本地变量？
    // Y: 只在栈顶找
    // N: 正常找
    auto res = force_local ? (
        variable_stack.back().count(name)
        ? variable_stack.back()[name]
        : std::make_pair<llvm::AllocaInst *, DataType>(nullptr, DataType::Int32)
    ) : find_variable(name);

    // 没有找到就分配新变量
    if(!res.first) {
        llvm::AllocaInst *ptr = current_builder->CreateAlloca(
            to_llvm_type(type, taichi_llvm_unit->context)
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
    // 保存常规的函数信息
    this->name = function_name;
    this->return_type = return_type;
    
    this->argument_list.clear();
    this->variable_stack.clear();
    for(auto arg : argument_list) {
        this->argument_list.push_back(arg);　// 参数列表
    }

    this->current_module = std::make_unique<llvm::Module>(
        "taichi_module_" + function_name,
        *(taichi_llvm_unit->context)
    );
    this->current_builder = std::make_unique< llvm::IRBuilder<> >(
        *(taichi_llvm_unit->context)
    );
    this->current_blocks = std::stack<llvm::BasicBlock *>();

    llvm::Type *llvm_return_type = to_llvm_type(
        this->return_type,
        taichi_llvm_unit->context
    );
    std::vector<llvm::Type *> llvm_args_type;
    for(auto arg : this->argument_list) {
        llvm_args_type.push_back(to_llvm_type(arg.type, taichi_llvm_unit->context));
    }
    llvm::ArrayRef<llvm::Type *> llvm_args_type_array(llvm_args_type);

    // 创建 LLVM 的函数类型
    llvm::FunctionType *func_type = llvm::FunctionType::get(
        llvm_return_type,
        llvm_args_type_array,
        false
    );

    // 创建函数
    this->llvm_function = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        this->name,
        *(this->current_module)
    );

    // 创建起始代码块
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(
        *(taichi_llvm_unit->context),
        "function_entry",
        this->llvm_function
    );
    this->current_builder->SetInsertPoint(entry_block); // 从这个位置开始构建代码
    this->current_blocks.push(entry_block);

    // 第一个作用域，也就是函数最外部的作用域
    this->variable_stack.push_back(
        std::unordered_map<
            std::string,
            std::pair<llvm::AllocaInst *, DataType>
        >()
    );
    llvm::Function::arg_iterator arg_begin = this->llvm_function->arg_begin(); // 获取每个参数
    // 将每个实参都另外存储一份（这样做不是最佳做法）
    for(auto arg : this->argument_list) {
        auto ptr = alloc_variable(arg.name, arg.type);
        current_builder->CreateStore(arg_begin++, ptr);
    }
}

void Function::build_finish()
{
    if (llvm::verifyFunction(*llvm_function)) {
        Out::Log(pType::ERROR, "verify function failed");
    }

    // 将构建的 IR 结果输出到一个 string
    // 使用 llvm 提供的这个 raw_string_ostream
    std::string func_code;
    llvm::raw_string_ostream rso(func_code);
    current_module->print(rso, nullptr);
    std::string _m = std::string("code of ") + name + " is" + (char)10;
    _m += std::string(40, '=') + (char)10; // 40 个 '=' 的写法
    _m += func_code;
    _m += std::string(40, '=');
    Out::Log(pType::DEBUG, _m.c_str());

    // 添加 Module 到 EE
    taichi_llvm_unit->engine->addModule(std::move(current_module));

    // 完成 JIT 编译的最后阶段
    // 确保编译完成，代码已被完全生成
    // 因为刚才添加了一个 module 所以现在执行
    taichi_llvm_unit->engine->finalizeObject();

    Out::Log(pType::DEBUG, "function has been added to engine");
}

// loop 的栈操作比较繁琐，要注意
void Function::loop_begin(
    const std::string &loop_index_name,
    int32_t l,
    int32_t r,
    int32_t s
)
{
    // 创建循环基本上需要「3个Block」
    // 循环条件判断（cond）、循环体（body）、和循环后的代码（after）
    llvm::BasicBlock *if_blcok = llvm::BasicBlock::Create(
        *(taichi_llvm_unit->context),
        "loop_if", // 这些名字不会影响逻辑，只是为了辅助调试（这些名字会在IR中保留）
        this->llvm_function
    );
    llvm::BasicBlock *body_block = llvm::BasicBlock::Create(
        *(taichi_llvm_unit->context),
        "loop_body",
        this->llvm_function
    );
    llvm::BasicBlock *next_block = llvm::BasicBlock::Create(
        *(taichi_llvm_unit->context),
        "loop_next",
        this->llvm_function
    );

    // 创建一个新的变量作用域（循环体 body 的作用域）
    variable_stack.push_back(
        std::unordered_map<
            std::string,
            std::pair<llvm::AllocaInst *, DataType>
        >()
    );
    // 注意对 block 的操作
    current_blocks.pop();
    current_blocks.push(next_block);
    current_blocks.push(if_blcok);
    current_blocks.push(body_block);

    auto loop_index_ptr = alloc_variable(loop_index_name, DataType::Int32, true); // 强制声明一个 local 变量（不使用外部变量）
    current_builder->CreateStore(
        llvm::ConstantInt::get(
            to_llvm_type(DataType::Int32, taichi_llvm_unit->context),
            static_cast<uint64_t>(l),
            true
        ),
        loop_index_ptr
    );
    // 保存当前 loop 的状态
    current_loop_update.push(std::make_pair(
        loop_index_ptr,
        s
    ));
    current_builder->CreateBr(if_blcok); // 无条件跳转
    // 跳转是一种「终结指令」
    // 每个基本块只能有一个终结指令（如 br、ret 等）（必须在最后吗？应该是的）

    // 开始构建 if 代码块
    current_builder->SetInsertPoint(if_blcok);
    llvm::LoadInst *load_loop_index = current_builder->CreateLoad(
        to_llvm_type(DataType::Int32, taichi_llvm_unit->context),
        loop_index_ptr
    );
    llvm::Value *compare_result = current_builder->CreateICmpSLT( // 创建比较节点
        load_loop_index,
        llvm::ConstantInt::get(
            to_llvm_type(DataType::Int32, taichi_llvm_unit->context),
            static_cast<uint64_t>(r),
            true
        )
    );
    // 根据条件跳转，进入循环 or 跳出循环
    current_builder->CreateCondBr(compare_result, body_block, next_block);
    // if 代码块 构建完成

    // 开始构建 body 代码块
    current_builder->SetInsertPoint(body_block);
}

void Function::loop_finish()
{
    // loop 结束的时候，loop index 要增加一个步进的长度
    llvm::LoadInst *load = current_builder->CreateLoad(
        to_llvm_type(DataType::Int32, taichi_llvm_unit->context),
        current_loop_update.top().first
    );
    llvm::Value *add_result = current_builder->CreateAdd(
        load,
        llvm::ConstantInt::get(
            to_llvm_type(DataType::Int32, taichi_llvm_unit->context),
            static_cast<uint64_t>(current_loop_update.top().second),
            true
        )
    );
    current_builder->CreateStore(
        add_result,
        current_loop_update.top().first
    );
    current_blocks.pop();
    current_builder->CreateBr(current_blocks.top()); // loop 结束之后 一定是跳转到 if 块
    current_blocks.pop();

    // loop 的作用域结束了
    variable_stack.pop_back();
    current_loop_update.pop();
    
    current_builder->SetInsertPoint(current_blocks.top()); // loop 结束之后的代码块
}

void Function::assignment_statement(
    const std::string &name,
    const OperationValue &value
)
{
    if(name == value.variable_name) return; // 同名赋值

    auto target_find_result = find_variable(name);
    if(!target_find_result.first) {
        alloc_variable(name, value.get_data_type(this)); // 找不到，就说明是新变量，在这里创建这个新变量
    }

    target_find_result = find_variable(name);
    current_builder->CreateStore(
        cast( // 在 Store 之前，需要进行类型转换
            value.get_data_type(this),
            target_find_result.second,
            value.construct_llvm_value(
                this,
                current_builder.get(),
                taichi_llvm_unit->context
            ),
            current_builder.get(),
            taichi_llvm_unit->context
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
        // 如果要创建新变量存储计算结果，新变量的类型需要计算得到（自动类型提升）
        DataType result_type = calc_type(left_type, right_type);

        alloc_variable(result_name, result_type);
    }

    find_result = find_variable(result_name);
    DataType result_type = find_result.second;

    // 运算之前，必须转换为相同的类型
    llvm::Value *llvm_left_value = cast(
        left_value.get_data_type(this),
        result_type,
        left_value.construct_llvm_value(
            this,
            current_builder.get(),
            taichi_llvm_unit->context
        ),
        current_builder.get(),
        taichi_llvm_unit->context
    );

    llvm::Value *llvm_right_value = cast(
        right_value.get_data_type(this),
        result_type,
        right_value.construct_llvm_value(
            this,
            current_builder.get(),
            taichi_llvm_unit->context
        ),
        current_builder.get(),
        taichi_llvm_unit->context
    );

    // 数据存储的时候 类型不区分是否有符号
    // 运算的时候才区分 比如CreateSDiv 是有符号的整数除法
    llvm::Value *llvm_result = nullptr;
    switch(operation_type) {
        case OperationType::Add:
            if(is_int(result_type)) {
                llvm_result = current_builder->CreateAdd( // 每种运算都有对应的 Create
                    llvm_left_value,
                    llvm_right_value
                );
            } else if(is_float(result_type)) {
                llvm_result = current_builder->CreateFAdd( // 浮点数加法
                    llvm_left_value,
                    llvm_right_value
                );
            }
            break;
        case OperationType::Sub: // 减法
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
        case OperationType::Mul: // 乘法
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
                llvm_result = current_builder->CreateSDiv( // S 表示有符号，这里是有符号的整数除法
                    llvm_left_value,
                    llvm_right_value
                );
            } else if(is_float(result_type)) {
                llvm_result = current_builder->CreateFDiv( // 浮点数除法
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
        // 找到这个变量之后 Load
        llvm::LoadInst *load_value = current_builder->CreateLoad(
            to_llvm_type(
                find_result.second,
                taichi_llvm_unit->context
            ),
            find_result.first
        );
        // Load 之后 Cast 类型转换
        llvm::Value *cast_value = cast(
            find_result.second,
            return_type,
            load_value,
            current_builder.get(),
            taichi_llvm_unit->context
        );
        // 之后返回这个 Value
        current_builder->CreateRet(cast_value);
    } else {
        current_builder->CreateRet(llvm_default_value(
            return_type,
            taichi_llvm_unit->context
        ));
    }
}

// 这个函数不能执行，主要是由于EE->runFunction的限制
std::shared_ptr<Byte[]> Function::run(Byte *argument_buffer, Byte *result_buffer)
{
    Byte *local_result_buffer = new Byte[type_size(return_type)];

    uint32_t offset = 0;
    uint32_t cache_32 = 0;
    uint64_t cache_64 = 0;
    std::vector<llvm::GenericValue> args;
    // 解析传入的参数值，构建一个 LLVM 的参数列表
    for(auto arg : this->argument_list) {
        args.push_back(llvm::GenericValue());
        // 创建一个 GenericValue
        switch(arg.type) {
            case DataType::Int32:
                memcpy(&cache_32, argument_buffer + offset, 4);
                args.back().IntVal = llvm::APInt(
                    32,
                    static_cast<uint64_t>(cache_32),
                    true
                );
                break;
            case DataType::Int64:
                memcpy(&cache_64, argument_buffer + offset, 8);
                args.back().IntVal = llvm::APInt(
                    64,
                    cache_64,
                    true
                );
                break;
            case DataType::Float32:
                memcpy(&cache_32, argument_buffer + offset, 4);
                args.back().FloatVal = reinterpret_cast<float&>(cache_32);
                break;
            case DataType::Float64:
                memcpy(&cache_64, argument_buffer + offset, 8);
                args.back().DoubleVal = reinterpret_cast<double&>(cache_64);
                break;
        }
        offset += type_size(arg.type);
    }
    llvm::ArrayRef<llvm::GenericValue> args_array(args);

    if(this->llvm_function) {
        // runFunction 不是官方推荐的调用函数的方式
        // 并且只能传递基本类型的参数
        // 实践中，调用这个函数会报错，提示不要这样调用
        // LLVM ERROR: MCJIT::runFunction does not support full-featured argument passing.
        // Please use ExecutionEngine::getFunctionAddress and cast the result to the desired function pointer type.
        llvm::GenericValue result = taichi_llvm_unit->engine->runFunction(
            this->llvm_function,
            args_array
        );

        // 获得返回值
        uint64_t result_value = result.IntVal.getZExtValue();
        Byte *src_buffer = (Byte *)&result_value;
        memcpy(local_result_buffer, src_buffer, type_size(return_type)); // CHECK
        memcpy(result_buffer, src_buffer, type_size(return_type));
    }

    // 把返回值直接当作 Bytes 返回，这里不做类型解析（也没办法做）
    return std::shared_ptr<Byte[]>(local_result_buffer, std::default_delete<Byte[]>());
}

}