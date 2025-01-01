# 示例代码

import taichi as ti

@ti.func
def calc(i: int) -> int:
    res = 0
    for j in range(999):
        res += j * j * i
    return res

@ti.log_time # 注意这两个修饰器的顺序 不能反过来
@ti.kernel
def calc_ti(n, data):
    for i in range(n):
        data[i] = calc(i)

@ti.log_time
def calc_cpu(n, data):
    for i in range(n):
        data[i] = calc(i)

def main():
    N = int(4e4)
    data = [0] * N
    calc_ti(N, data)
    calc_cpu(N, data)

def test_1(func):
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    
    return wrapper

@test_1
def test_2():
    for i in range(5):
        print(i)

if __name__ == "__main__":
    # main()

    test_2()