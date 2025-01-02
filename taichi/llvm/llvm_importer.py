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

c_function_entry = lib_llvm_taichi.function_entry
c_function_entry.argtypes = tuple()
c_function_entry.restype = None

c_function_exit = lib_llvm_taichi.function_exit
c_function_exit.argtypes = tuple()
c_function_exit.restype = None