import sys
import enum

_cfg = dict()

class cfg(enum.Enum):
    bytes_order = "bytes_order"
    bytes_order_c = "bytes_order_c"

def cfg_set(key: cfg, value):
    _cfg[key.value] = value

def cfg_get(key: cfg):
    return _cfg[key.value]

cfg_set(cfg.bytes_order, sys.byteorder)

if cfg_get(cfg.bytes_order) == "big":
    cfg_set(cfg.bytes_order_c, ">")
else:
    cfg_set(cfg.bytes_order_c, "<")