# 示例代码

import taichi as ti

# @ti.func
# def calc(i: ti.Int32, offset: ti.Int64) -> ti.Int64:
#     res = 0
#     for j in range(100):
#         res = res - 1
#         for k in range(0, 200, 2):
#             s = j + k
#             x = s + i
#             y = x * offset
#             res = res + y
#     return res

# # 注意这两个修饰器的顺序 不能反过来
# @ti.log_time
# @ti.kernel
# def calc_ti(n, data, offset = 3):
#     for i in range(n):
#         data[i] = calc(i, offset)

# @ti.log_time
# def calc_no(n, data, offset = 3):
#     for i in range(n):
#         data[i] = calc(i, offset)

@ti.func
def add(x: ti.Int32, y: ti.Int32) -> ti.Int32:
    res = x + y
    return res

def main():
    N = int(4e2)
    # data1, data2 = [0] * N, [0] * N
    # calc_ti(N, data1, offset=5)
    # calc_no(N, data2, offset=5)

    # ti.log_message(
    #     "result check passed"
    #     if sum([1 if data1[i] == data2[i] else 0 for i in range(N)]) == N
    #     else
    #     "result check failed"
    # )

if __name__ == "__main__":
    ti.log_message(add(1,1))
    # main()