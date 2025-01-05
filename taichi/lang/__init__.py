import os
import ast
import ctypes
import taichi.type
import taichi.llvm
from taichi.lang.numba import ReplaceTopRangeToPrange
from taichi.tool import *

# 本模块的内容用于处理 AST

# 遍历所有的函数调用
# 注意这里的基类是 NodeVisitor 而不是 NodeTransformer
# 因为我们只需要遍历获得信息 而不用修改原本的内容
class FunctionCallVisitor(ast.NodeVisitor):
    def __init__(self, function_calls: set):
        super().__init__()
        # 用于存储结果
        self.function_calls = function_calls

    # 用于遍历 Call 节点
    def visit_Call(self, node):
        # 通过名字调用一个函数
        if isinstance(node.func, ast.Name):
            self.function_calls.add(node.func.id)
        # 通过成员属性调用一个函数（也就是调用成员函数）
        elif isinstance(node.func, ast.Attribute):
            ... # TODO
        # 注意要递归调用 遍历子节点
        self.generic_visit(node)

def convert_kernel_main_loop_to_func(
        func: ast.FunctionDef
    ) -> ast.FunctionDef:

    main_loop = None
    for stmt in func.body:
        if (
            main_loop is None
            and isinstance(stmt, ast.For)
            and isinstance(stmt.iter, ast.Call)
            and isinstance(stmt.iter.func, ast.Name)
            and stmt.iter.func.id == "range"
        ):
            main_loop = stmt
        else:
            log_warning(
                f"illegal code has been ignored of kernel {func.name}{os.linesep}{ast.unparse(stmt)}"
            )
    
    if main_loop is None:
        log_warning(f"kernel {func.name} is empty")
        return None

    if len(main_loop.iter.args) == 1:
        loop_range = [
            ast.Constant(value=0),
            main_loop.iter.args[0],
            ast.Constant(value=1)
        ]
    elif len(main_loop.iter.args) == 2:
        loop_range = [
            main_loop.iter.args[0],
            main_loop.iter.args[1],
            ast.Constant(value=1)
        ]
    elif len(main_loop.iter.args) == 3:
        loop_range = main_loop.iter.args
    else:
        log_error(f"the args of range loop in kernel {func.name} is illegal")
        return None

    args = func.args
    args.args.extend([
        ast.arg(arg="_taichi_thread_id", annotation=None),
        ast.arg(arg="_taichi_thread_cnt", annotation=None)
    ])
    args.defaults.extend([
        ast.Constant(value=0),
        ast.Constant(value=1)
    ])

    body = [ast.For(
        target=ast.Name(id=main_loop.target.id, ctx=ast.Store()),
        iter=ast.Call(
            func=ast.Name(id="range", ctx=ast.Load()),
            args=[
                ast.BinOp(
                    left=loop_range[0],
                    op=ast.Add(),
                    right=ast.BinOp(
                        left=ast.Name(id="_taichi_thread_id", ctx=ast.Load()),
                        op=ast.Mult(),
                        right=loop_range[2]
                    )
                ),
                loop_range[1],
                ast.BinOp(
                    left=loop_range[2],
                    op=ast.Mult(),
                    right=ast.Name(id="_taichi_thread_cnt", ctx=ast.Load())
                )
            ],
            keywords=[]
        ),
        body=main_loop.body,
        orelse=[]
    )]

    result_func = ast.FunctionDef(
        name="_taichi_working_thread_func",
        args=args,
        body=body,
        decorator_list=[]
    )
    ast.fix_missing_locations(result_func)

    return result_func

def _body_filter(target: list, source: list, depth: int = 0):
    for stmt in source:
        if isinstance(stmt, ast.Assign):
            if (
                len(stmt.targets) != 1
                or not isinstance(stmt.targets[0], ast.Name)
            ):
                log_warning(
                    f"ignore this assign for multi targets{os.linesep}{ast.unparse(stmt)}"
                )
                continue
            if isinstance(stmt.value, ast.Constant):
                target.append(stmt)
            elif (
                isinstance(stmt.value, ast.BinOp)
                and (
                    isinstance(stmt.value.left, ast.Name)
                    or isinstance(stmt.value.left, ast.Constant)
                )
                and (
                    isinstance(stmt.value.right, ast.Name)
                    or isinstance(stmt.value.right, ast.Constant)
                )
                and (
                    isinstance(stmt.value.op, ast.Add)
                    or isinstance(stmt.value.op, ast.Sub)
                    or isinstance(stmt.value.op, ast.Mult)
                    or isinstance(stmt.value.op, ast.Div)
                )
            ):
                target.append(stmt)
        elif isinstance(stmt, ast.For):
            if (
                isinstance(stmt.iter, ast.Call)
                and isinstance(stmt.iter.func, ast.Name)
                and stmt.iter.func.id == "range"
                and sum([
                    0
                    if isinstance(arg, ast.Constant)
                    else
                    1
                    for arg in stmt.iter.args
                ]) == 0
            ):
                for_body = []
                _body_filter(for_body, stmt.body, depth = depth + 1)
                target.append(ast.For(
                    target=stmt.target,
                    iter=stmt.iter,
                    body=for_body,
                    orelse=[]
                ))
        elif depth == 0 and isinstance(stmt, ast.Return):
            if isinstance(stmt.value, ast.Name):
                target.append(stmt)
                break

def convert_func_to_pure_calc_task(
    func: ast.FunctionDef
) -> ast.FunctionDef:
    args = func.args
    args_list = []
    for arg in args.args:
        if (
            isinstance(arg.annotation, ast.Attribute)
            and arg.annotation.attr in taichi.type.basic_types
        ):
            args_list.append(arg)
    args.args = args_list
    args.kw_defaults = []
    args.defaults = []

    if (
        isinstance(func.returns, ast.Attribute)
        and func.returns.attr in taichi.type.basic_types
    ):
        returns = func.returns
    else:
        log_error(f"func {func.name} needs return taichi basic type")
        return None

    body = []
    _body_filter(body, func.body)
    
    result_func = ast.FunctionDef(
        name=func.name,
        args=args,
        body=body,
        decorator_list=[],
        returns=returns
    )

    ast.fix_missing_locations(result_func)

    return result_func

def build_llvm_func(func: ast.FunctionDef):
    args = func.args
    returns = func.returns
    
    # function_name_b = func.name.encode(encoding="ascii")
    # taichi.llvm.c_function_begin(
    #     BP(function_name_b),
    #     0,
    #     BP(function_name_b),
    #     BP(function_name_b),
    #     0
    # )