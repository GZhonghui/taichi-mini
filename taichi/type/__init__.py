# 类型定义
import struct

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

# sync with cpp
type_id = {
    Int32.__name__: 1,
    Int64.__name__: 2,
    Float32.__name__: 3,
    Float64.__name__: 4
}

def to_bytes(value, type: str) -> bytes:
    if type == Int32.__name__:
        return int(value).to_bytes(4, byteorder="big", signed=True)
    elif type == Int64.__name__:
        return int(value).to_bytes(8, byteorder="big", signed=True)
    elif type == Float32.__name__:
        return struct.pack("f", value)
    elif type == Float64.__name__:
        return struct.pack("d", value)
    
def from_bytes(bytes: bytes, type: str):
    if type == Int32.__name__:
        return int.from_bytes(bytes, byteorder="big", signed=True)
    elif type == Int64.__name__:
        return int.from_bytes(bytes, byteorder="big", signed=True)
    elif type == Float32.__name__:
        return struct.unpack("f", bytes)
    elif type == Float64.__name__:
        return struct.unpack("d", bytes)