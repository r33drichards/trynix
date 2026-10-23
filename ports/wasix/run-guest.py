#!/usr/bin/env python3
"""Bounded local cold-boot/Lua gate. Requests no new network/root/env grants.
--offline is registry-only; use external network isolation for strict denial."""
import argparse
import json
import math
import os
from pathlib import Path
import resource
import selectors
import signal
import subprocess
import time

p=argparse.ArgumentParser()
p.add_argument('wasm',type=Path);p.add_argument('pack',type=Path);p.add_argument('share',type=Path)
p.add_argument('--verbose-kernel',action='store_true')
p.add_argument('--timeout',type=float,default=300);p.add_argument('--log',type=Path,default=Path('guest-boot.log'))
a=p.parse_args()
if not math.isfinite(a.timeout) or a.timeout <= 0:
 p.error("timeout must be finite and positive")
machine=json.loads((a.pack/'machine.json').read_text())
args=[x.replace('{pack}','/pack').replace('{share}','/share').replace('{ram}','512M').replace('tcg,tb-size=500','tcg,tb-size=16') for x in machine['args']]
if a.verbose_kernel:
 args=[x.replace('loglevel=4','loglevel=7 earlyprintk=serial,ttyS0,115200') for x in args]
cmd=['wasmer','run','--offline','--no-tty','--volume',f'{a.pack.resolve()}:/pack','--volume',f'{a.share.resolve()}:/share',str(a.wasm.resolve()),'--',*args]
start=time.monotonic(); output=bytearray(); handshake=False; submitted=False; result='incomplete'
# Validate/open output before starting a potentially long-lived child.
live=a.log.open('wb')
proc=None;sel=None
try:
 proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
 sel=selectors.DefaultSelector();sel.register(proc.stdout,selectors.EVENT_READ)
 while time.monotonic()-start<a.timeout:
  for key,_ in sel.select(timeout=.2):
   chunk=os.read(key.fd,65536)
   if chunk:
    output.extend(chunk); live.write(chunk); live.flush()
  if len(output)>8*1024**2: result='output_limit';break
  if not handshake and b'trynix: waiting for the store' in output:
   proc.stdin.write(b'ready\n');proc.stdin.flush();handshake=True
  if not submitted and b'trynix: welcome to the multiverse' in output:
   # Marker is assembled by Lua, not present literally in the echoed command.
   proc.stdin.write(b"lua -e 'print(string.char(76,85,65,95,72,69,76,76,79,95,80,65,83,83))'; echo LUA_EXIT:$?\n")
   proc.stdin.flush();submitted=True
  if b'LUA_HELLO_PASS' in output and b'LUA_EXIT:0' in output: result='passed';break
  if proc.poll() is not None: result='exited_before_pass';break
 else: result='deadline'
finally:
 if proc is not None:
  try: os.killpg(proc.pid,signal.SIGKILL)
  except ProcessLookupError: pass
  proc.wait()
 if sel is not None: sel.close()
 live.close()
report={'result':result,'linux_handshake':handshake,'lua_submitted':submitted,'elapsed_seconds':round(time.monotonic()-start,3),'returncode':proc.returncode,'host_peak_rss_kib':resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,'wasm_bytes':a.wasm.stat().st_size,'guest_ram_mib':512,'tcg_cache_mib':16,'log':str(a.log),'scope':'local cold boot; not snapshot restore or Edge validation'}
print(json.dumps(report,indent=2));raise SystemExit(0 if result=='passed' else 1)
