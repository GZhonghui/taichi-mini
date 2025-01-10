# 定义运算类型
# 为了描述「表达式」

import ast

__all__ = []

# sync with cpp
operation_id = {
    "Add": 1,
    "Sub": 2,
    "Mul": 3,
    "Div": 4
}

# 为了 AST 和 taichi 的体系统一
# sync with cpp
def ast_operation_id(op) -> int:
    if isinstance(op, ast.Add):
        return 1
    elif isinstance(op, ast.Sub):
        return 2
    elif isinstance(op, ast.Mult):
        return 3
    elif isinstance(op, ast.Div):
        return 4
    else:
        return 0