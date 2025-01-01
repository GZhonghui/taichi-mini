import ast
import inspect
import threading

threading_number = 4

def proc_func(node: ast.FunctionDef):
    res = list()

    for stmt in node.body:
        if (
            isinstance(stmt, ast.For)
            and isinstance(stmt.iter, ast.Call)
            and isinstance(stmt.iter.func, ast.Name)
            and stmt.iter.func.id == "range"
        ):
            if len(stmt.iter.args) == 1:
                args = [
                    ast.Constant(value=0),
                    stmt.iter.args[0],
                    ast.Constant(value=1)
                ]
            elif len(stmt.iter.args) == 2:
                args = [
                    stmt.iter.args[0],
                    stmt.iter.args[1],
                    ast.Constant(value=1)
                ]
            elif len(stmt.iter.args) == 3:
                args = stmt.iter.args
            else:
                print("error args")
            res.append(ast.FunctionDef(
                name=f"_taichi_inner_loop_{len(res)}",
                args=node.args,
                body=[stmt],
                decorator_list=[]
            ))
        else:
            ...
            # print(ast.unparse(stmt))

    return res

def test_1(func):
    source_code = inspect.getsource(func)
    tree = ast.parse(source_code)

    for node in ast.walk(tree):
        if isinstance(node, ast.FunctionDef) and node.name == func.__name__:
            node.decorator_list = []
            res = proc_func(node)
            for i in res:
                ast.fix_missing_locations(i)
                print(ast.unparse(i))
            break
            # assign_node = ast.Assign(
            #     targets=[ast.Name(id="x", ctx=ast.Store())],
            #     value=ast.Constant(value=0)
            # )
            # node.body.insert(0, assign_node)
            
        if (
            isinstance(node, ast.For)
            and isinstance(node.iter, ast.Call)
            and isinstance(node.iter.func, ast.Name)
            and node.iter.func.id == "range"
        ):
            continue
            if len(node.iter.args) == 1:
                args = [
                    ast.Constant(value=0),
                    node.iter.args[0],
                    ast.Constant(value=1)
                ]
            elif len(node.iter.args) == 2:
                args = [
                    node.iter.args[0],
                    node.iter.args[1],
                    ast.Constant(value=1)
                ]
            elif len(node.iter.args) == 3:
                args = node.iter.args
            else:
                print("error args")

            for i in range(threading_number):
                for_node = ast.For(
                    target=ast.Name(id=node.target.id, ctx=ast.Store()),
                    iter=ast.Call(
                        func=ast.Name(id="range", ctx=ast.Load()),
                        args=[
                            ast.BinOp(
                                left=args[0],
                                op=ast.Add(),
                                right=ast.BinOp(
                                    left=args[2],
                                    op=ast.Mult(),
                                    right=ast.Name(
                                        id="_taichi_threading_id",
                                        ctx=ast.Load()
                                    )
                                )
                            ),
                            args[1],
                            ast.BinOp(
                                left=args[2],
                                op=ast.Mult(),
                                right=ast.Constant(value=threading_number)
                            )
                        ],
                        keywords=[]
                    ),
                    body=node.body,
                    orelse=[]
                )
                ast.fix_missing_locations(for_node)
                print(ast.unparse(for_node))
            
            

            continue
            args = [
                i.value
                for i in node.iter.args
                if isinstance(i, ast.Constant)
            ]
            if (
                len(args) != len(node.iter.args)
                or len(args) > 3
                or len(args) < 1
            ):
                continue

            l, r, s = [
                0, args[0], 1
            ] if len(args) == 1 else [
                args[0], args[1], 1
            ] if len(args) == 2 else args

            for i in range(threading_number):
                print(l + i * s, r, s * threading_number)

            print(l, r, s)
            for_node = ast.For(
                target=ast.Name(id=node.target.id, ctx=ast.Store()),
                iter=ast.Call(
                    func=ast.Name(id="range", ctx=ast.Load()),
                    args=[
                        ast.Constant(value=l),
                        ast.Constant(value=r),
                        ast.Constant(value=s)
                    ],
                    keywords=[]
                ),
                body=node.body,
                orelse=[]
            )
            ast.fix_missing_locations(for_node)
            print(ast.unparse(for_node))
            print([
                ast.unparse(i)
                for i in node.body
            ])

    print(ast.dump(tree, indent=4))
    # ast.fix_missing_locations(tree)
    # print(ast.unparse(tree))

    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    
    return wrapper

@test_1
def test_2(n: int, data: list):
    for i in range(0, n, 2):
        data[i] = 0
        _i = i
        while _i > 0 and _i % 2 == 0:
            data[i] += 1
            _i = _i // 2

if __name__ == "__main__":
    n = 10
    res = [0] * n
    test_2(n, res)
    print(res)