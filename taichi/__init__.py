import ast # 抽象语法树 Abstract Syntax Tree
import inspect # 用于获取 Python 对象的信息
import numba # 外部库 用于编译 Python Code

from taichi.tool import *
import taichi.lang
import taichi.core

# 模仿 taichi 的 kernel 修饰器
def kernel(func):
    def wrapper(*args, **kwargs):
        # 获取这个函数的源码
        # 注意：获取的源码是包含装饰器的
        source_code = inspect.getsource(func)
        # 将源码解析成 AST
        tree = ast.parse(source_code)

        # 在 AST 上运行我们写的规则
        # 将顶层的 for-range 替换为 for-prange
        transformer = taichi.lang.ReplaceTopRangeToPrange()
        # 得到一棵新的 AST
        transformed_tree = transformer.visit(tree)

        # 在 AST 中找到我们的函数
        # 并将其所有装饰器都移除掉
        # 因为我们接下来要编译代码了 不能留着原来的装饰器
        # 这样会导致一个问题：在 ti.kernel 之前不能有其他装饰器
        # 也就是说 ti.kernel 必须是第一个作用的装饰器（最靠近函数）
        # TODO: 这里有没有更加优雅的实现方式？
        for node in ast.walk(transformed_tree):
            if isinstance(node, ast.FunctionDef) and node.name == func.__name__:
                node.decorator_list = []

        # 用于在对 AST 树进行修改后修复可能缺失的位置信息（lineno 和 col_offset 属性）
        # lineno：表示节点在源代码中的行号
        # col_offset：表示节点在该行中的列偏移
        # 这些信息在生成错误信息或者 unparse 的时候会很有用
        ast.fix_missing_locations(transformed_tree)

        # 编译经过处理的 AST
        code_obj = compile(transformed_tree, filename="<ast>", mode="exec")

        # 创建一个空字典 作为新的运行环境
        blank_namespace = {}
        # 引入需要的内容
        blank_namespace["prange"] = numba.prange

        # TODO: 如果只导入 numba 的话貌似不行
        # 在内部访问不到 numba.prange
        # blank_namespace["numba"] = numba

        used_funcs = set()
        func_visitor = taichi.lang.FunctionCallVisitor(used_funcs)
        func_visitor.visit(transformed_tree)

        for func_name in used_funcs:
            func_obj = taichi.core.get_func("global", func_name)
            if func_obj is not None:
                blank_namespace[func_name] = func_obj

        exec(code_obj, blank_namespace)

        transformed_func = blank_namespace[func.__name__]

        # transformed_func = numba.njit(parallel=True)(transformed_func)

        return transformed_func(*args, **kwargs)
    wrapper.__name__ = func.__name__
    return wrapper

# 模仿 taichi 的 func 修饰器
def func(f):
    def wrapper(*args, **kwargs):
        return f(*args, **kwargs)
    # 将包装后的函数的 name 设定为和原函数一致
    wrapper.__name__ = f.__name__
    # 使用 setattr 为这个函数设定一个属性 标记这个函数是 taichi 的 func
    # 使用 getattr 可以获取属性
    setattr(wrapper, 'is_taichi_func', True)

    # 注册这个函数
    # 使用「文件名」（这里没做完）和「函数名」作为 key 保存这个函数
    # 后面使用「文件名和函数名」可以重新取得这个函数
    taichi.core.register_func(
        "global", # TODO
        # 获取这个函数定义所在的文件
        # inspect.getfile(f),
        f.__name__,
        wrapper
    )

    return wrapper