# 这里记录所有的 func 函数
# 一个函数使用了 @ti.func 修饰器的话
# 在修饰器中 就会将其注册到这个 table 里面
# 逻辑很简单
# 简单来说就是使用两个 key 记录一个函数对象
_taichi_func_table = dict()

# 注册
# 注册的函数对象是经过包装的 wrapper
def register_func(
    file_path: str,
    func_name: str,
    func
):
    global _taichi_func_table
    if file_path not in _taichi_func_table:
        _taichi_func_table[file_path] = dict()
    _taichi_func_table[file_path][func_name] = func

# 获取
def get_func(
    file_path: str,
    func_name: str
):
    global _taichi_func_table
    if file_path not in _taichi_func_table:
        return None
    if func_name not in _taichi_func_table[file_path]:
        return None
    return _taichi_func_table[file_path][func_name]