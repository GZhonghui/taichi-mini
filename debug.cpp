#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetSelect.h"

#include <iostream>
#include <functional>

int main() {
    // 初始化 LLVM 环境
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // 创建 LLVM 上下文、模块和 IR 构建器
    llvm::LLVMContext Context;
    auto Module = std::make_unique<llvm::Module>("simple_module", Context);
    llvm::IRBuilder<> Builder(Context);

    // 创建函数签名：int add(int, int)
    llvm::Type *Int32Type = llvm::Type::getInt32Ty(Context);
    llvm::FunctionType *FuncType = llvm::FunctionType::get(Int32Type, {Int32Type, Int32Type}, false);
    llvm::Function *AddFunc = llvm::Function::Create(FuncType, llvm::Function::ExternalLinkage, "add", *Module);

    // 定义函数体
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(Context, "entry", AddFunc);
    Builder.SetInsertPoint(BB);
    llvm::Argument *Arg1 = AddFunc->getArg(0);
    llvm::Argument *Arg2 = AddFunc->getArg(1);
    llvm::Value *Sum = Builder.CreateAdd(Arg1, Arg2, "sum");
    Builder.CreateRet(Sum);

    // 验证模块
    if (llvm::verifyFunction(*AddFunc)) {
        llvm::errs() << "Function verification failed!\n";
        return 1;
    }

    Module->print(llvm::outs(), nullptr);

    // 使用 JIT 执行函数
    std::string ErrStr;
    std::unique_ptr<llvm::ExecutionEngine> EE(
        llvm::EngineBuilder(std::move(Module))
            .setErrorStr(&ErrStr)
            .create());

    if (!EE) {
        llvm::errs() << "Failed to create ExecutionEngine: " << ErrStr << "\n";
        return 1;
    }

    EE->finalizeObject();
    EE->finalizeObject();

    // 获取函数地址
    uint64_t AddFuncAddr = EE->getFunctionAddress("add");
    if (!AddFuncAddr) {
        llvm::errs() << "Failed to get function address!\n";
        return 1;
    }

    // 将地址转换为函数指针
    using AddFuncType = int (*)(int, int);
    AddFuncType Add = reinterpret_cast<AddFuncType>(reinterpret_cast<void *>(AddFuncAddr));

    // 调用函数
    int Result = Add(10, 20);

    // 打印结果
    std::cout << "Result: " << Result << std::endl;

    return 0;
}