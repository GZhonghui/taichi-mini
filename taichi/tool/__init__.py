# 一些工具函数

__all__ = [
    "BP",
    "log_debug",
    "log_message",
    "log_warning",
    "log_error",
    "log_time",
    "log_levels",
    "log_set_level",
    "log_get_level",
    "cfg",
    "cfg_get",
    "cfg_set"
]

import ctypes
from taichi.tool.log import log_debug, log_message, log_warning, log_error, log_time
from taichi.tool.log import log_levels, log_set_level, log_get_level
from taichi.tool.config import cfg, cfg_get, cfg_set

def BP(bytes: bytes):
    return ctypes.cast(bytes, ctypes.POINTER(ctypes.c_uint8))