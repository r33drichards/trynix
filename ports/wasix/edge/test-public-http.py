"""Actual synchronous QEMU/Linux/Lua HTTP smoke test."""
import base64,json,os,resource,signal,subprocess,time,urllib.request,urllib.error
from pathlib import Path
import argparse
p=argparse.ArgumentParser()
for name in ('wasm','qemu','pack','share','out'):p.add_argument(name,type=Path)
a=p.parse_args();out=a.out;out.mkdir()
machine=json.loads((a.pack/'machine.json').read_text())
args=[x.replace('{pack}','/pack').replace('{share}','/share').replace('{ram}','512M').replace('tcg,tb-size=500','tcg,tb-size=16').replace('security_model=none','security_model=mapped-file,readonly=on') for x in machine['args']]
cmd=['wasmer','run','--offline','--net','--no-tty','--env','PUBLIC_POC=1','--env',f'POC_EXPIRES_AT={int(time.time())+300}','--env','PORT=18082','--volume',f'{a.qemu.resolve().parent}:/emu','--volume',f'{a.pack.resolve()}:/pack','--volume',f'{a.share.resolve()}:/share',str(a.wasm.resolve()),'--','/emu/'+a.qemu.name,*args]
start=time.monotonic();report={};log=(out/'sync-linux.log').open('wb');p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
try:
 for _ in range(100):
  try:
   with urllib.request.urlopen('http://127.0.0.1:18082/health',timeout=3) as r:assert r.status==200
   break
  except urllib.error.URLError:time.sleep(.1)
 report['health_seconds']=time.monotonic()-start
 source=b'assert(io.open("/dev/tty","w")==nil); io.write("SYNC_LUA:",io.read("*a"),string.char(0)); io.stderr:write("ERR")'
 request=urllib.request.Request('http://127.0.0.1:18082/execute',method='POST',data=source+b'input',headers={'X-Source-Length':str(len(source))})
 with urllib.request.urlopen(request,timeout=220) as r:report['http_status']=r.status;report['response']=json.load(r)
 v=report['response'];assert v['state']==3 and v['exit_code']==0 and v['reaped'],v
 raw=base64.b64decode(v['output_base64']);assert b'SYNC_LUA:input\0' in raw and b'ERR' in raw,raw
 report['result']='passed'
except Exception as e:report.update(result='failed',error=str(e))
finally:
 os.killpg(p.pid,signal.SIGKILL);p.wait();log.close();report['elapsed_seconds']=time.monotonic()-start;report['host_peak_rss_kib']=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
 (out/'sync-linux-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='passed' else 1)
