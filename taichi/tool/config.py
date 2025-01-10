# 管理配置

import sys
import enum

# 把 set get 函数暴露出去
# 数据留在这个模块
# 这是一种很常见的 单例 设计模式
# python 的模块本质上是单例的 所以很适合用于这种方式
_cfg = dict()

# 在 python 中使用枚举
# 需要注意的是
# cfg.bytes_order == "cfg.bytes_order"
# cfg.bytes_order.value == "bytes_order"
# 明确自己需要用的是哪个
class cfg(enum.Enum): # 继承这个类
    bytes_order = "bytes_order"
    bytes_order_c = "bytes_order_c"

def cfg_set(key: cfg, value):
    _cfg[key.value] = value

def cfg_get(key: cfg):
    return _cfg[key.value]

# 获取系统默认的大小端设定
cfg_set(cfg.bytes_order, sys.byteorder)

if cfg_get(cfg.bytes_order) == "big":
    cfg_set(cfg.bytes_order_c, ">")
else:
    cfg_set(cfg.bytes_order_c, "<")