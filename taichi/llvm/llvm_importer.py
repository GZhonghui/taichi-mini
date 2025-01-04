import os
import ctypes
from ctypes import POINTER, c_uint8, c_uint32, c_uint64

__all__ = [
    "c_function_entry",
    "c_function_exit"
]

current_path = os.path.dirname(os.path.abspath(__file__))
so_path = os.path.join(current_path, "llvm_taichi.so")
lib_llvm_taichi = ctypes.cdll.LoadLibrary(so_path)

c_init_lib = lib_llvm_taichi.init_lib
c_init_lib.argtypes = ()
c_init_lib.restype = None
c_init_lib()