# 示例代码

import taichi as ti
ti.init()

@ti.func
def calc_ti(i: ti.Int64, magic: ti.Int64) -> ti.Int64:
    res = 0 # 常量赋值
    for j in range(100): # 常数范围的循环
        for k in range(0, 200, 2): # 循环可以嵌套
            a = i + j # 简单计算
            b = a + k
            c = b * magic
            res = res + c
    return res # 在最后返回一个变量

# 注意这两个修饰器的顺序 不能反过来
@ti.log_time
@ti.kernel
def task_ti(n, data, magic = 3): # kernel 内部只能包含 loop 结构
    for i in range(n):
        data[i] = calc_ti(i, magic)

@ti.log_time
def task_no(n, data, magic = 3):
    for i in range(n):
        data[i] = calc_ti.origin(i, magic)

def main():
    N = int(4e4)
    data1, data2 = [0] * N, [0] * N

    task_ti(N, data1, magic=5)
    task_no(N, data2, magic=5)

    # 验证结果
    ti.log_message(
        "result check passed"
        if sum([1 if data1[i] == data2[i] else 0 for i in range(N)]) == N
        else
        "result check failed"
    )

if __name__ == "__main__":
    main()