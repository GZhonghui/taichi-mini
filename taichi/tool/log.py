# 用于管理 log

import time
import enum

class log_levels(enum.Enum):
    debug = "debug"
    message = "message"
    warning = "warning"
    error = "error"

# sync with cpp
_log_level_id_table = {
    "debug": 1,
    "message": 2,
    "warning": 3,
    "error": 4
}

_log_level_id = 1

def log(*args, **kwargs):
    # get the current time and format it as [hh:mm:ss]
    timestamp = time.strftime("[%H:%M:%S]", time.localtime())
    # print the timestamp with all passed arguments, *args and **kwargs allow for any number of arguments
    print(timestamp, *args, **kwargs)

def log_debug(*args, **kwargs):
    if _log_level_id <= _log_level_id_table["debug"]:
        log("[ DEBUG ] >>", *args, **kwargs)

def log_message(*args, **kwargs):
    if _log_level_id <= _log_level_id_table["message"]:
        log("[MESSAGE] >>", *args, **kwargs)

def log_warning(*args, **kwargs):
    if _log_level_id <= _log_level_id_table["warning"]:
        log("[WARNING] >>", *args, **kwargs)

def log_error(*args, **kwargs):
    if _log_level_id <= _log_level_id_table["error"]:
        log("[ ERROR ] >>", *args, **kwargs)

# 一个装饰器 计算函数的运行时间
def log_time(func):
    def wrapper(*args, **kwargs):
        start_time = time.time()  # start timing
        result = func(*args, **kwargs)  # call the original function and get the result
        end_time = time.time()  # end timing
        duration = end_time - start_time  # calculate runtime
        # log the runtime using the log function
        log_message(f"function {func.__name__} took {duration:.6f} seconds to execute") 
        return result
    wrapper.__name__ = func.__name__
    return wrapper

# only works for python
def log_set_level(log_level: log_levels):
    if log_level.value not in _log_level_id_table.keys():
        return
    id = _log_level_id_table[log_level.value]

    global _log_level_id
    _log_level_id = id

def log_get_level() -> int:
    return _log_level_id