import ast
import inspect

class AddToSubTransformer(ast.NodeTransformer):
    def visit_BinOp(self, node):
        self.generic_visit(node)
        if isinstance(node.op, ast.Add):
            node.op = ast.Sub()
        return node

def kernel(func):
    def wrapper(*args, **kwargs):
        source_code = inspect.getsource(func)
        tree = ast.parse(source_code)

        transformer = AddToSubTransformer()
        transformed_tree = transformer.visit(tree)

        for node in ast.walk(transformed_tree):
            if isinstance(node, ast.FunctionDef) and node.name == func.__name__:
                node.decorator_list = []

        code_obj = compile(transformed_tree, filename="<ast>", mode="exec")

        blank_namespace = {}
        exec(code_obj, blank_namespace)

        transformed_func = blank_namespace[func.__name__]

        return transformed_func(*args, **kwargs)
    return wrapper