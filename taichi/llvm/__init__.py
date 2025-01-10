# LLVM 模块 主要由 CPP 实现

import ctypes
from taichi.llvm.llvm_importer import *

# 公开两个外部可调用函数

def set_lib_log_level(level_id: int):
    c_set_log_level(ctypes.c_uint8(level_id))

def init_lib():
    c_init_lib()