# 类型定义

__all__ = [
    "Int32",
    "Int64",
    "Float32",
    "Float64"
]

class BaseType:
    def __init__(self):
        self._type = "BaseType"

class Int32(BaseType):
    def __init__(self):
        super().__init__()
        self._type = "Int32"

class Int64(BaseType):
    def __init__(self):
        super().__init__()
        self._type = "Int64"

class Float32(BaseType):
    def __init__(self):
        super().__init__()
        self._type = "Float32"

class Float64(BaseType):
    def __init__(self):
        super().__init__()
        self._type = "Float64"

basic_types = [
    Int32.__name__,
    Int64.__name__,
    Float32.__name__,
    Float64.__name__
]