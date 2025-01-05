import os
import ast
import inspect
from ctypes import c_void_p, c_int32, CFUNCTYPE

from taichi.tool import *
import taichi.lang
import taichi.llvm
import taichi.core.func_manager
import taichi.type

# 模仿 taichi 的 func 修饰器
def func(f):
    source_code = inspect.getsource(f)
    tree = ast.parse(source_code)

    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == f.__name__:
            node.decorator_list = []
            pure_calc_task = taichi.lang.convert_func_to_pure_calc_task(node)
    if pure_calc_task:
        log_message(
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
        
        # test
        temp_name = "test_add"
        temp_name_b = temp_name.encode(encoding="ascii")
        temp_ptr = taichi.llvm.c_get_func_ptr(BP(temp_name_b))
        CFUNCTYPE_Add = CFUNCTYPE(c_int32, c_int32, c_int32)
        add_func = CFUNCTYPE_Add(temp_ptr)
        print(add_func(1,1))

        def wrapper(*args, **kwargs):
            # return add_func(c_int32(args[0]), c_int32(args[1]))
            args_bytes_list = list()
            for i in range(len(args_type)):
                args_bytes_list.append(
                    taichi.type.to_bytes(args[i], args_type[i])
                )
            args_b = b"".join(args_bytes_list)
            result_b = taichi.type.to_bytes(0, return_type)
            # taichi.llvm.c_run(
            #     BP(function_name_b),
            #     BP(args_b),
            #     BP(result_b)
            # )
            return taichi.type.from_bytes(result_b, return_type)

    else:
        log_error(f"func {f.__name__} compile failed")

        def wrapper(*args, **kwargs):
            return f(*args, **kwargs)

    # 将包装后的函数的 name 设定为和原函数一致
    wrapper.__name__ = f.__name__
    # 使用 setattr 为这个函数设定一个属性 标记这个函数是 taichi 的 func
    # 使用 getattr 可以获取属性
    setattr(wrapper, "is_taichi_func", True)

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