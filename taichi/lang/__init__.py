# 用于解析代码

import os
import ast
import struct
import taichi.type
import taichi.llvm
import taichi.lang.operation
from ctypes import POINTER, c_uint8, c_int32
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

# 一个 kernel 含有一个主要的 loop
# 传入一个 kernel
# 将 main-loop 的 body 包装为一个函数返回 用于多线程执行
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
    
    # 没找到 main-loop 就返回
    if main_loop is None:
        log_warning(f"kernel {func.name} is empty")
        return None

    # 根据 range 的参数得到循环范围
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

    # 拓展两个参数，用于指定线程ID
    args = func.args
    args.args.extend([
        ast.arg(arg="_taichi_thread_id", annotation=None),
        ast.arg(arg="_taichi_thread_cnt", annotation=None)
    ])
    args.defaults.extend([
        ast.Constant(value=0),
        ast.Constant(value=1)
    ])

    # 创建一个新的 FOR 循环，作为新函数的 body
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

    # 返回的是新函数
    result_func = ast.FunctionDef(
        name="_taichi_working_thread_func",
        args=args,
        body=body,
        decorator_list=[]
    )
    ast.fix_missing_locations(result_func)

    return result_func

# 对语句做筛查，只保留支持的语法
def _body_filter(target: list, source: list, depth: int = 0):
    for stmt in source:
        # 接受一部分赋值语句
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
            if isinstance(stmt.value, ast.Name):
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
        # 接受一部分 FOR 循环
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
                # FOR 循环的 body 要递归处理
                _body_filter(for_body, stmt.body, depth = depth + 1)
                target.append(ast.For(
                    target=stmt.target,
                    iter=stmt.iter,
                    body=for_body,
                    orelse=[]
                ))
        # 接受一部分 return 语句
        elif depth == 0 and isinstance(stmt, ast.Return):
            if isinstance(stmt.value, ast.Name):
                target.append(stmt)
                break

# 把一个 ti.func 转换为一个「单纯」的计算任务，也就是把不支持的语法都筛掉
def convert_func_to_pure_calc_task(
    func: ast.FunctionDef
) -> ast.FunctionDef:
    args = func.args
    args_list = []
    for arg in args.args:
        if (
            isinstance(arg.annotation, ast.Attribute)
            and arg.annotation.attr in taichi.type.basic_types # 基础类型的参数我们才要
        ):
            args_list.append(arg)
    args.args = args_list
    args.kw_defaults = []
    args.defaults = []

    # 检查返回值类型，必须是一个基础类型才行
    if (
        isinstance(func.returns, ast.Attribute)
        and func.returns.attr in taichi.type.basic_types
    ):
        returns = func.returns
    else:
        log_error(f"func {func.name} needs return taichi basic type")
        return None

    # 对 body 的语句做筛查
    body = []
    _body_filter(body, func.body)
    
    # 返回新函数
    result_func = ast.FunctionDef(
        name=func.name,
        args=args,
        body=body,
        decorator_list=[],
        returns=returns
    )

    # 自己创建的 AST 一般都需要调用此函数 否则会出错
    ast.fix_missing_locations(result_func)

    return result_func

# 获取函数原型，也就是参数列表 和 返回值类型
def get_func_prototype(func: ast.FunctionDef):
    args = func.args
    args_type_list = []
    for arg in args.args:
        args_type_list.append(arg.annotation.attr)
    return_type = func.returns.attr
    return args_type_list, return_type

# 将一个 Value 节点的信息包装为 bytes 方便传递给 C
# Value 可能是变量，此时字节中包含此变量的 name
# Value 可能是常量，此时字节中包含的信息是 type 和 value
# Value 出现在「赋值语句」或者「表达式」中
def _value_node_to_bytes(node) -> bytes:
    # 序列化一个常量
    if isinstance(node, ast.Constant):
        source_value = node.value
        if isinstance(source_value, int):
            buffer = [
                int(1).to_bytes(1, byteorder=cfg_get(cfg.bytes_order), signed=False),
                int(taichi.type.type_id[taichi.type.Int32.__name__]).to_bytes(
                    1, byteorder=cfg_get(cfg.bytes_order), signed=False
                ),
                source_value.to_bytes(4, byteorder=cfg_get(cfg.bytes_order), signed=True)
            ]
        elif isinstance(source_value, float):
            buffer = [
                int(1).to_bytes(1, byteorder=cfg_get(cfg.bytes_order), signed=False),
                int(taichi.type.type_id[taichi.type.Float32.__name__]).to_bytes(
                    1, byteorder=cfg_get(cfg.bytes_order), signed=False
                ),
                struct.pack(f"{cfg_get(cfg.bytes_order_c)}f", source_value,)
            ]
        else:
            log_error(source_value, "is not a acceptable constant")
    # 序列化一个变量
    elif isinstance(node, ast.Name):
        buffer = [
            int(0).to_bytes(1, byteorder=cfg_get(cfg.bytes_order), signed=False),
            int(0).to_bytes(1, byteorder=cfg_get(cfg.bytes_order), signed=False),
            node.id.encode(encoding="ascii")
        ]
    buffer_b = b"".join(buffer)
    return buffer_b

# 在 LLVM 端构建函数体的内容
def _build_body(func_name_b: bytes, body: list):
    # 遍历 AST 的内容，调用相对应的 C 接口函数，在 C 端创建对应的语句
    for stmt in body:
        if isinstance(stmt, ast.For):
            loop_index_name_b = stmt.target.id.encode(encoding="ascii")
            iter_args = stmt.iter.args
            if len(iter_args) == 1:
                loop_range = [0, iter_args[0].value, 1]
            elif len(iter_args) == 2:
                loop_range = [iter_args[0].value, iter_args[1].value, 1]
            elif len(iter_args) == 3:
                loop_range = [i.value for i in iter_args]
            # 创建一个 FOR 循环
            taichi.llvm.c_loop_begin(
                BP(func_name_b),
                BP(loop_index_name_b),
                c_int32(loop_range[0]),
                c_int32(loop_range[1]),
                c_int32(loop_range[2])
            )
            # FOR 循环的 body 要递归处理
            _build_body(func_name_b, stmt.body)
            # 循环要显式结束
            taichi.llvm.c_loop_finish(BP(func_name_b))
        elif isinstance(stmt, ast.Assign):
            target_name_b = stmt.targets[0].id.encode(encoding="ascii")
            if (
                isinstance(stmt.value, ast.Constant)
                or isinstance(stmt.value, ast.Name)
            ):
                buffer_b = _value_node_to_bytes(stmt.value)
                # 创建一个赋值语句，右侧是单个项目（常量或变量）
                taichi.llvm.c_assignment_statement_value(
                    BP(func_name_b),
                    BP(target_name_b),
                    BP(buffer_b)
                )
            elif isinstance(stmt.value, ast.BinOp):
                left_b = _value_node_to_bytes(stmt.value.left)
                right_b = _value_node_to_bytes(stmt.value.right)
                # 创建一个赋值语句，右侧是表达式
                taichi.llvm.c_assignment_statement_operation(
                    BP(func_name_b),
                    BP(target_name_b),
                    BP(left_b),
                    c_uint8(taichi.lang.operation.ast_operation_id(stmt.value.op)),
                    BP(right_b)
                )
        # 创建一个 return 语句
        elif isinstance(stmt, ast.Return):
            return_name_b = stmt.value.id.encode(encoding="ascii")
            taichi.llvm.c_return_statement(
                BP(func_name_b),
                BP(return_name_b)
            )

# 构造一个 LLVM 函数（使用 C 提供的接口，在 C 端构建）
def build_llvm_func(func: ast.FunctionDef):
    function_name_b = func.name.encode(encoding="ascii")

    args = func.args
    args_number = len(args.args)
    args_type = list()
    args_name = str()
    for arg in args.args:
        args_type.append(taichi.type.type_id[arg.annotation.attr].to_bytes(
            1, byteorder=cfg_get(cfg.bytes_order), signed=False
        ))
        args_name += arg.arg + ","
    args_type_b = b"".join(args_type)
    args_name_b = args_name.encode(encoding="ascii")
    # 函数名，参数列表，返回值，提供这些信息以开始一个函数
    taichi.llvm.c_function_begin(
        BP(function_name_b),
        c_uint8(args_number),
        BP(args_type_b),
        BP(args_name_b),
        c_uint8(taichi.type.type_id[func.returns.attr])
    )

    # 构建函数体是递归进行的
    _build_body(function_name_b, func.body)

    # 显式表明结束这个函数，交给 LLVM 编译代码
    taichi.llvm.c_function_finish(
        BP(function_name_b)
    )