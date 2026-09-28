#!/usr/bin/env python3
import argparse, hashlib, pathlib
PATTERNS={
  'BlockTessellator::_getTexture':('FF C3 01 D1 FD 7B 02 A9 F9 1B 00 F9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 83 00 91 59 D0 3B D5 F3 03 05 AA F4 03 04 2A',0xA67583C),
  'BlockTessellatorCache::getBlock':('FD 7B BB A9 F9 0B 00 F9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 03 00 91 F4 03 00 AA 00 00 40 F9',0xA65CAD4),
  'BlockGraphics::getTextureUVCoordinateSet':('FF 83 01 D1 FD 7B 02 A9 F8 5F 03 A9 F6 57 04 A9 F4 4F 05 A9 FD 83 00 91 58 D0 3B D5 F4 03 08 AA F7 03 00 AA 08 17 40 F9 A8 83 1F F8',0xA66EB14),
  'BlockTessellatorPipeline::useNewTessellation':('FD 7B BE A9 F4 4F 01 A9 FD 03 00 91 F4 03 01 2A F3 03 00 AA DE 63 33 94',0xA66F20C),
}
def all_offsets(data, needle):
  out=[]; pos=0
  while True:
    i=data.find(needle,pos)
    if i<0:return out
    out.append(i); pos=i+1
p=argparse.ArgumentParser(); p.add_argument('lib'); a=p.parse_args()
data=pathlib.Path(a.lib).read_bytes()
print('size=',len(data)); print('sha256=',hashlib.sha256(data).hexdigest())
for name,(pat,expected) in PATTERNS.items():
  off=all_offsets(data,bytes.fromhex(pat))
  print(f'{name}: matches={len(off)} offsets={[hex(x) for x in off]} expected={hex(expected)}', 'OK' if off==[expected] else 'MISMATCH')
