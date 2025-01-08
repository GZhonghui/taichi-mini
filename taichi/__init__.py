from taichi.core import kernel
from taichi.core import func

from taichi.tool import *
from taichi.type import *

import taichi.llvm as _llvm

def init(
    log_level:log_levels = log_levels.message
):
    # 设定 log 等级
    log_set_level(log_level)
    _llvm.set_lib_log_level(log_get_level())
    _llvm.init_lib() # 初始化 C lib
    log_message("Taichi inited")