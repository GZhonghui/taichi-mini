import os
import ast
import inspect
from ctypes import c_void_p, c_int32, CFUNCTYPE

from taichi.tool import *
import taichi.lang
import taichi.llvm
import taichi.type
import taichi.core.func_manager

# 模仿 taichi 的 func 修饰器
def func(f):
    # 获取目标函数的 AST
    source_code = inspect.getsource(f)
    tree = ast.parse(source_code)

    for node in ast.walk(tree):
        # 找到函数定义
        if isinstance(node, ast.FunctionDef) and node.name == f.__name__:
            # 去掉装饰器
            node.decorator_list = []
            # 检查一遍语法，不支持的语法都去掉
            pure_calc_task = taichi.lang.convert_func_to_pure_calc_task(node)
    # 检查完成，可以开始使用 LLVM 构建函数
    if pure_calc_task:
        log_debug(
            f"analyze results of function {f.__name__}{os.linesep}"
            f"{'=' * 40}{os.linesep}"
            f"{ast.unparse(pure_calc_task)}{os.linesep}"
            f"{'=' * 40}"
        )

        # 获取函数原型
        args_type, return_type = taichi.lang.get_func_prototype(pure_calc_task)

        # 构建函数
        taichi.lang.build_llvm_func(pure_calc_task)

        function_name = f.__name__
        function_name_b = function_name.encode(encoding="ascii")

        # 获取这个函数在 C 端的指针（这个是原始指针）
        function_ptr = taichi.llvm.c_get_func_ptr(BP(function_name_b))
        function_prototype = [
            taichi.type.type_to_ctypes[i]
            for i in [return_type, *args_type]
        ]

        # CFUNCTYPE 用于定义函数指针类型
        # _CFUNCTYPE 是我们创建的一个「函数类型」
        # function_ptr 是从C拿到的一个函数指针
        # 将「函数指针」转换为正确的类型之后就可以调用了
        _CFUNCTYPE = CFUNCTYPE(*function_prototype)
        # 这里，终于编译完成
        # self_func 就是编译结果，它是机器字节码，可以在 python 调用
        self_func = _CFUNCTYPE(function_ptr)

        # 说实话，CFUNCTYPE很关键，这一步转换「函数指针类型」在C端很难实现

        # wrapper 的工作就是
        # 转换一下 参数 和 返回值 的类型，调用 self_func
        def wrapper(*args, **kwargs):
            cast_args = []
            for i in range(len(args_type)):
                cast_args.append(
                    taichi.type.type_to_ctypes[args_type[i]](
                        taichi.type.cast(args[i], args_type[i])
                    )
                )
            result = self_func(*cast_args)
            return taichi.type.type_to_ctypes[return_type](result).value

    # 构建失败，原函数 f 就保持不变
    else:
        log_error(f"func {f.__name__} compile failed")

        def wrapper(*args, **kwargs):
            return f(*args, **kwargs)

    # 将包装后的函数的 name 设定为和原函数一致
    wrapper.__name__ = f.__name__
    # 使用 setattr 为这个函数设定一个属性 标记这个函数是 taichi 的 func
    # 使用 getattr 可以获取属性
    setattr(wrapper, "is_taichi_func", True)

    # 保留一下原函数，测试使用
    wrapper.origin = f

    # 注册这个函数
    # 使用「文件名」（这里没做完）和「函数名」作为 key 保存这个函数
    # 后面使用「文件名和函数名」可以重新取得这个函数
    taichi.core.func_manager.register_func(
        "global", # TODO
        # 获取这个函数定义所在的文件
        # inspect.getfile(f),
        f.__name__,
        wrapper
    )

    return wrapper