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
    source_code = inspect.getsource(f)
    tree = ast.parse(source_code)

    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == f.__name__:
            node.decorator_list = []
            pure_calc_task = taichi.lang.convert_func_to_pure_calc_task(node)
    if pure_calc_task:
        log_debug(
            f"analyze results of function {f.__name__}{os.linesep}"
            f"{'=' * 40}{os.linesep}"
            f"{ast.unparse(pure_calc_task)}{os.linesep}"
            f"{'=' * 40}"
        )

        args_type, return_type = taichi.lang.get_func_prototype(pure_calc_task)

        taichi.lang.build_llvm_func(pure_calc_task)

        function_name = f.__name__
        function_name_b = function_name.encode(encoding="ascii")

        function_ptr = taichi.llvm.c_get_func_ptr(BP(function_name_b))
        function_prototype = [
            taichi.type.type_to_ctypes[i]
            for i in [return_type, *args_type]
        ]

        _CFUNCTYPE = CFUNCTYPE(*function_prototype)
        self_func = _CFUNCTYPE(function_ptr)

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

    else:
        log_error(f"func {f.__name__} compile failed")

        def wrapper(*args, **kwargs):
            return f(*args, **kwargs)

    # 将包装后的函数的 name 设定为和原函数一致
    wrapper.__name__ = f.__name__
    # 使用 setattr 为这个函数设定一个属性 标记这个函数是 taichi 的 func
    # 使用 getattr 可以获取属性
    setattr(wrapper, "is_taichi_func", True)

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