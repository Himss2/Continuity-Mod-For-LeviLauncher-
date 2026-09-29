#!/usr/bin/env python3
import argparse
import hashlib
import pathlib

PATTERNS = {
    "BlockTessellator::_getTexture": (
        "FF C3 01 D1 FD 7B 02 A9 F9 1B 00 F9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 83 00 91 59 D0 3B D5 F3 03 05 AA F4 03 04 2A",
        0xA67583C,
    ),
    "BlockTessellatorCache::getBlock": (
        "FD 7B BB A9 F9 0B 00 F9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 03 00 91 F4 03 00 AA 00 00 40 F9",
        0xA65CAD4,
    ),
}

def all_offsets(data: bytes, needle: bytes):
    out = []
    pos = 0
    while True:
        i = data.find(needle, pos)
        if i < 0:
            return out
        out.append(i)
        pos = i + 1

parser = argparse.ArgumentParser()
parser.add_argument("lib")
args = parser.parse_args()

data = pathlib.Path(args.lib).read_bytes()
print("size=", len(data))
print("sha256=", hashlib.sha256(data).hexdigest())

for name, (pattern, expected) in PATTERNS.items():
    offsets = all_offsets(data, bytes.fromhex(pattern))
    status = "OK" if offsets == [expected] else "MISMATCH"
    print(
        f"{name}: matches={len(offsets)} "
        f"offsets={[hex(x) for x in offsets]} "
        f"expected={hex(expected)} {status}"
    )
