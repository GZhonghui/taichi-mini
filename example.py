import taichi as ti

@ti.kernel
def sum(n):
    res = 0
    for i in range(n):
        res = res + i
    return res

def main():
    print(sum(10))

if __name__ == "__main__":
    main()