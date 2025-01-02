import ast

# 本 class 已经弃用 因为依赖于 numba
# 而出于学习目的 本项目尽可能手写 已经不再使用 numba
# 仅出于学习目的保留此段代码

# 找到最外层的 for-range 循环
# 并将其替换为 for-prange
# 注意：最外层的循环可能不止有一个 有多个的话全部都要替换
class ReplaceTopRangeToPrange(ast.NodeTransformer):
    def __init__(self):
        super().__init__()
        # 用于维护当前是否在最外层
        self.is_top_level = True

    # 本函数用于遍历 for 循环（的节点）
    # visit_xxx 一般就是用于遍历 xxx 节点的函数
    # 具体命名可以查看文档
    def visit_For(self, node):
        if (
            # 如果是最外层
            self.is_top_level
            # 是一个函数调用
            and isinstance(node.iter, ast.Call)
            # 并且函数是通过名字调用的
            # 这排除了复杂表达式或属性调用的情况
            and isinstance(node.iter.func, ast.Name)
            # 并且函数名是 range
            and node.iter.func.id == "range"
        ):
            # 这里替换成 numba 的 prange 循环 用于并行化
            # 这里也可以自己处理 写自己的并行化的代码
            node.iter.func.id = "prange"
        
        was_top_level = self.is_top_level
        # 下一层一定不是最外层
        self.is_top_level = False
        # 注意要递归调用 遍历子节点
        self.generic_visit(node)

        # 退出前 将这个变量的值恢复回来
        # 因为可能有多个顶层的 for 循环
        self.is_top_level = was_top_level
        return node