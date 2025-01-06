import os
import ctypes
from ctypes import POINTER, c_uint8, c_int32, c_void_p

__all__ = [
    "c_init_lib",
    "c_set_log_level",
    "c_function_begin",
    "c_function_finish",
    "c_loop_begin",
    "c_loop_finish",
    "c_assignment_statement_value",
    "c_assignment_statement_operation",
    "c_return_statement",
    "c_run",
    "c_get_func_ptr"
]

current_path = os.path.dirname(os.path.abspath(__file__))
so_path = os.path.join(current_path, "llvm_taichi.so")
lib_llvm_taichi = ctypes.cdll.LoadLibrary(so_path)

c_init_lib = lib_llvm_taichi.init_lib
c_init_lib.argtypes = ()
c_init_lib.restype = None

c_set_log_level = lib_llvm_taichi.set_log_level
c_set_log_level.argtypes = (c_uint8,)
c_set_log_level.restype = None

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
c_function_finish.argtypes = (
    POINTER(c_uint8), # function_name
)
c_function_finish.restype = None

c_loop_begin = lib_llvm_taichi.loop_begin
c_loop_begin.argtypes = (
    POINTER(c_uint8), # function_name
    POINTER(c_uint8), # loop_index_name
    c_int32, # l
    c_int32, # r
    c_int32 # s
)
c_loop_begin.restype = None

c_loop_finish = lib_llvm_taichi.loop_finish
c_loop_finish.argtypes = (
    POINTER(c_uint8), # function_name
)
c_loop_finish.restype = None

c_assignment_statement_value = lib_llvm_taichi.assignment_statement_value
c_assignment_statement_value.argtypes = (
    POINTER(c_uint8), # function_name
    POINTER(c_uint8), # target_variable_name
    POINTER(c_uint8) # source_buffer
)
c_assignment_statement_value.restype = None

c_assignment_statement_operation = lib_llvm_taichi.assignment_statement_operation
c_assignment_statement_operation.argtypes = (
    POINTER(c_uint8), # function_name
    POINTER(c_uint8), # target_variable_name
    POINTER(c_uint8), # left_buffer
    c_uint8, # operation_type
    POINTER(c_uint8) # right_buffer
)
c_assignment_statement_operation.restype = None

c_return_statement = lib_llvm_taichi.return_statement
c_return_statement.argtypes = (
    POINTER(c_uint8), # function_name
    POINTER(c_uint8) # return_variable_name
)
c_return_statement.restype = None

c_run = lib_llvm_taichi.run
c_run.argtypes = (
    POINTER(c_uint8), # function_name
    POINTER(c_uint8), # argument_buffer
    POINTER(c_uint8) # result_buffer
)
c_run.restype = None

c_get_func_ptr = lib_llvm_taichi.get_func_ptr
c_get_func_ptr.argtypes = (
    POINTER(c_uint8), # function_name
)
c_get_func_ptr.restype = c_void_p
