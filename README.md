# taichi-mini

太极（[Taichi](https://github.com/taichi-dev/taichi)）很方便，一个装饰器就可以将 Python 代码加速数十倍。

但这是怎么做到的？

通过翻阅太极的文档和代码，我实现了这个迷你版的太极，它也可以帮你加速代码。

当然，它不支持GPU、不支持复杂的语法、没有数学库、加速效果也没有那么好。

它不是为了用于实际的生产环境，而是尝试用 2k 行左右的代码，介绍一种编译 Python 代码的方式。

## 运行
```
# 需要首先安装好 LLVM
make
python example.py

# 在 Apple M1 芯片上的执行结果，大约加速了 60 倍
[16:20:37] [MESSAGE] >> Taichi inited
[16:20:37] [MESSAGE] >> function task_ti took 0.415144 seconds to execute
[16:21:02] [MESSAGE] >> function task_no took 24.548944 seconds to execute
[16:21:02] [MESSAGE] >> result check passed
```