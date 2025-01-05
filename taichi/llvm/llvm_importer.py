import os
import ctypes
from ctypes import POINTER, c_uint8, c_uint32, c_uint64

__all__ = [
    "c_function_begin",
    "c_function_finish"
]

current_path = os.path.dirname(os.path.abspath(__file__))
so_path = os.path.join(current_path, "llvm_taichi.so")
lib_llvm_taichi = ctypes.cdll.LoadLibrary(so_path)

c_init_lib = lib_llvm_taichi.init_lib
c_init_lib.argtypes = ()
c_init_lib.restype = None
c_init_lib()

c_function_begin = lib_llvm_taichi.function_begin
c_function_begin.argtypes = (
    POINTER(c_uint8), # function_name
    c_uint8, # args_number
    POINTER(c_uint8), # args_type
    POINTER(c_uint8), # args_name
    c_uint8 # return_type
)
c_function_begin.restype = None

c_function_finish = lib_llvm_taichi.function_finish
c_function_finish.argtypes = ()
c_function_finish.restype = None
