import time

# 一些工具函数

__all__ = [
    "log_message",
    "log_warning",
    "log_error",
    "log_time"
]

def log(*args, **kwargs):
    # get the current time and format it as [hh:mm:ss]
    timestamp = time.strftime("[%H:%M:%S]", time.localtime())
    # print the timestamp with all passed arguments, *args and **kwargs allow for any number of arguments
    print(timestamp, *args, **kwargs)

def log_message(*args, **kwargs):
    log("[MESSAGE]", *args, **kwargs)

def log_warning(*args, **kwargs):
    log("[WARNING]", *args, **kwargs)

def log_error(*args, **kwargs):
    log("[ ERROR ]", *args, **kwargs)

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