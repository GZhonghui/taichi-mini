# 类型定义
import ctypes
import struct
from taichi.tool import *

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

type_to_ctypes = {
    Int32.__name__: ctypes.c_int32,
    Int64.__name__: ctypes.c_int64,
    Float32.__name__: ctypes.c_float,
    Float64.__name__: ctypes.c_double
}

def cast(value, type: str):
    if (
        type == Int32.__name__
        or type == Int64.__name__
    ):
        return int(value)
    elif (
        type == Float32.__name__
        or type == Float64.__name__
    ):
        return float(value)

def to_bytes(value, type: str) -> bytes:
    if type == Int32.__name__:
        return int(value).to_bytes(4, byteorder=cfg_get(cfg.bytes_order), signed=True)
    elif type == Int64.__name__:
        return int(value).to_bytes(8, byteorder=cfg_get(cfg.bytes_order), signed=True)
    elif type == Float32.__name__:
        return struct.pack(f"{cfg_get(cfg.bytes_order_c)}f", value)
    elif type == Float64.__name__:
        return struct.pack(f"{cfg_get(cfg.bytes_order_c)}d", value)
    
def from_bytes(bytes: bytes, type: str):
    if type == Int32.__name__:
        return int.from_bytes(bytes, byteorder=cfg_get(cfg.bytes_order), signed=True)
    elif type == Int64.__name__:
        return int.from_bytes(bytes, byteorder=cfg_get(cfg.bytes_order), signed=True)
    elif type == Float32.__name__:
        return struct.unpack("f", bytes)
    elif type == Float64.__name__:
        return struct.unpack("d", bytes)