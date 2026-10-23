#!/usr/bin/env python3
"""Remove only Wasm debug custom sections, without rewriting instructions/features."""
import sys
from pathlib import Path
source=Path(sys.argv[1]).read_bytes()
if source[:8] != b'\x00asm\x01\x00\x00\x00': raise ValueError('Not Wasm v1')
def leb(pos):
 value=0
 for shift in range(0,35,7):
  b=source[pos];pos+=1;value|=(b&127)<<shift
  if b<128:return value,pos
 raise ValueError('Invalid u32 LEB')
out=bytearray(source[:8]);pos=8
while pos<len(source):
 start=pos;kind=source[pos];size,payload=leb(pos+1);end=payload+size
 if end>len(source):raise ValueError('Truncated section')
 remove=False
 if kind==0:
  n,at=leb(payload)
  if at+n>end:raise ValueError('Invalid custom section name')
  name=source[at:at+n]
  remove=name==b'name' or name.startswith(b'.debug_') or name==b'sourceMappingURL'
 if not remove:out.extend(source[start:end])
 pos=end
Path(sys.argv[2]).write_bytes(out)
print(f'{len(source)} -> {len(out)} bytes')
