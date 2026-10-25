#!/usr/bin/env python3
"""Two sequential disposable guest lifecycles. Sanity tests, not escape-proof security."""
from pathlib import Path
import argparse,subprocess,selectors,signal,os,time,json,resource
p=argparse.ArgumentParser();p.add_argument('wasm',type=Path);p.add_argument('pack',type=Path);p.add_argument('share',type=Path);p.add_argument('out',type=Path);a=p.parse_args();a.out.mkdir()
args=[x.replace('{pack}','/pack').replace('{share}','/share').replace('{ram}','512M').replace('tcg,tb-size=500','tcg,tb-size=16').replace('security_model=none','security_model=mapped-file,readonly=on') for x in json.loads((a.pack/'machine.json').read_text())['args']]
cmd=['wasmer','run','--offline','--no-tty','--volume',f'{a.pack.resolve()}:/pack','--volume',f'{a.share.resolve()}:/share',str(a.wasm.resolve()),'--',*args]
def marker(s):return 'print(string.char('+','.join(str(x) for x in s.encode())+'))'
def run(number):
 start=time.monotonic();proc=None;sel=None;buf=bytearray();sent=set();result={'instance':number,'steps':{},'result':'incomplete'};cancel_at=None
 log=(a.out/f'instance-{number}.log').open('wb')
 def send(key,command):
  proc.stdin.write(command.encode()+b'\n');proc.stdin.flush();sent.add(key)
 try:
  proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True);sel=selectors.DefaultSelector();sel.register(proc.stdout,selectors.EVENT_READ)
  while time.monotonic()-start<180:
   for key,_ in sel.select(.1):
    data=os.read(key.fd,65536);buf.extend(data);log.write(data);log.flush()
   now=time.monotonic()
   try:
    status=Path(f'/proc/{proc.pid}/status').read_text();rss=next(int(x.split()[1]) for x in status.splitlines() if x.startswith('VmRSS:'))
    if rss>1887436:raise RuntimeError('RSS ceiling')
   except FileNotFoundError:pass
   if len(buf)>8*1024**2:raise RuntimeError('Output ceiling')
   if b'trynix: waiting for the store' in buf and 'ready' not in sent:send('ready','ready')
   if b'trynix: welcome to the multiverse' in buf and 'first' not in sent:
    if number==1:send('first',"lua -e '"+marker('WARM_ONE')+"'; printf canary > /tmp/wasix-lifecycle-canary")
    else:send('first',"test ! -e /tmp/wasix-lifecycle-canary && lua -e '"+marker('FRESH_INSTANCE_PASS')+"'")
   if number==1:
    if b'WARM_ONE' in buf and 'second' not in sent:
     result['steps']['first_lua_seconds']=round(now-start,3)
     send('second',"test -f /tmp/wasix-lifecycle-canary && lua -e '"+marker('WARM_TWO')+"'")
    if b'WARM_TWO' in buf and 'bounds' not in sent:
     result['steps']['warm_second_seconds']=round(now-start,3)
     # Root is guest initramfs, not host root; export is guest-read-only.
     send('bounds',"test ! -e /workspace && ! touch /share/forbidden-write && lua -e '"+marker('GUEST_BOUNDARY_PASS')+"'")
    if b'GUEST_BOUNDARY_PASS' in buf and 'busy' not in sent:
     result['steps']['guest_boundary_seconds']=round(now-start,3)
     send('busy',"lua -e '"+marker('BUSY_STARTED')+"; io.stdout:flush(); while true do end'")
    if b'BUSY_STARTED' in buf and cancel_at is None:cancel_at=now+2;result['steps']['busy_started_seconds']=round(now-start,3)
    if cancel_at is not None and now>=cancel_at:result['result']='cancelled_busy_guest';break
   elif b'FRESH_INSTANCE_PASS' in buf:result['result']='fresh_after_cancel_pass';break
   if proc.poll() is not None:raise RuntimeError(f'Unexpected guest exit {proc.returncode}')
  else:raise RuntimeError('Deadline')
 except Exception as e:result['error']=str(e);result['result']='failed'
 finally:
  stop=time.monotonic()
  if proc is not None:
   try:os.killpg(proc.pid,signal.SIGKILL)
   except ProcessLookupError:pass
   proc.wait(timeout=10);result['returncode']=proc.returncode
   try:os.killpg(proc.pid,0);result['process_group_gone']=False
   except ProcessLookupError:result['process_group_gone']=True
   result['cleanup_seconds']=round(time.monotonic()-stop,3)
  if sel:sel.close()
  log.close()
 result['elapsed_seconds']=round(time.monotonic()-start,3);(a.out/f'instance-{number}.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True);return result
results=[run(1)]
if results[0]['result']=='cancelled_busy_guest' and results[0].get('process_group_gone'):results.append(run(2))
passed=len(results)==2 and results[1]['result']=='fresh_after_cancel_pass' and results[1].get('process_group_gone')
report={'passed':passed,'scope':'guest sanity isolation/warm reuse/process cancellation; not hostile escape resistance, Edge or snapshots','results':results,'cumulative_child_peak_rss_kib':resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss};(a.out/'report.json').write_text(json.dumps(report,indent=2));raise SystemExit(0 if passed else 1)
