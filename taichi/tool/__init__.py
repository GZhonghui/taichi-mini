import time

# 一些工具函数

__all__ = [
    "log",
    "log_time"
]

def log(*args, **kwargs):
    # get the current time and format it as [hh:mm:ss]
    timestamp = time.strftime("[%H:%M:%S]", time.localtime())
    # print the timestamp with all passed arguments, *args and **kwargs allow for any number of arguments
    print(timestamp, *args, **kwargs)

def log_time(func):
    def wrapper(*args, **kwargs):
        start_time = time.time()  # start timing
        result = func(*args, **kwargs)  # call the original function and get the result
        end_time = time.time()  # end timing
        duration = end_time - start_time  # calculate runtime
        log(f"function {func.__name__} took {duration:.6f} seconds to execute")  # log the runtime using the log function
        return result
    return wrapper