"""Tiny fake-child absolute-expiry controls, not Edge evidence."""
import json,os,signal,socket,subprocess,time,urllib.request,urllib.error
from pathlib import Path
import argparse
p=argparse.ArgumentParser()
for name in ('wasm','fake','out'):p.add_argument(name,type=Path)
a=p.parse_args();out=a.out;out.mkdir();build=a.fake.resolve().parent
base=['wasmer','run','--offline','--net','--no-tty','--env','PUBLIC_POC=1','--env','PORT=18083','--volume',f'{build}:/probe']
end=[str(a.wasm.resolve()),'--','/probe/'+a.fake.name]
report={}
for name,extra in [('invalid',['--env','POC_EXPIRES_AT=invalid']),('too_far',['--env',f'POC_EXPIRES_AT={int(time.time())+7200}'])]:
 r=subprocess.run(base+extra+end,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=10);assert r.returncode!=0,(name,r.stdout);report[name+'_fails_closed']=True
persistent_log=(out/'persistent-test.log').open('wb')
process=subprocess.Popen(base+end,stdout=persistent_log,stderr=subprocess.STDOUT,start_new_session=True)
try:
 for _ in range(100):
  try:
   with urllib.request.urlopen('http://127.0.0.1:18083/health',timeout=1) as r:
    assert r.status==200;break
  except urllib.error.URLError:time.sleep(.05)
 else:raise AssertionError('persistent startup failed')
 assert process.poll() is None
 report['missing_expiry_starts_persistent']=True
finally:
 os.killpg(process.pid,signal.SIGKILL);process.wait();persistent_log.close()
log=(out/'expiry-test.log').open('wb');expires=int(time.time())+3
p=subprocess.Popen(base+['--env',f'POC_EXPIRES_AT={expires}']+end,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
def req(path,body=None):
 q=urllib.request.Request('http://127.0.0.1:18083'+path,data=body,headers={'X-Source-Length':str(len(body))} if body is not None else {})
 try:
  with urllib.request.urlopen(q,timeout=10) as r:return r.status,json.load(r)
 except urllib.error.HTTPError as e:return e.code,json.load(e)
try:
 for _ in range(100):
  try:req('/health');break
  except urllib.error.URLError:time.sleep(.05)
 start=time.monotonic();code,r=req('/execute',b'BUSY');assert code==200 and r['state']==4 and r['reaped'],(code,r)
 report['active_expiry_cancels_reaps']=True;report['elapsed_seconds']=time.monotonic()-start
 code,r=req('/execute',b'PASS');assert code==410 and r['error']=='POC expired',(code,r)
 report['expired_admission_410']=True;report['result']='passed'
finally:
 os.killpg(p.pid,signal.SIGKILL);p.wait();log.close();(out/'expiry-test-result.json').write_text(json.dumps(report,indent=2));print(report)
