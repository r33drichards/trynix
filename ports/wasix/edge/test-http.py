import argparse,base64,json,os,resource,signal,subprocess,time,urllib.request,urllib.error
from pathlib import Path
parser=argparse.ArgumentParser()
for name in ('wasm','qemu','pack','share','out'):parser.add_argument(name,type=Path)
a=parser.parse_args();out=a.out;out.mkdir()
machine=json.loads((a.pack/'machine.json').read_text())
args=[x.replace('{pack}','/pack').replace('{share}','/share').replace('{ram}','512M').replace('tcg,tb-size=500','tcg,tb-size=16').replace('security_model=none','security_model=mapped-file,readonly=on') for x in machine['args']]
cmd=['wasmer','run','--offline','--net','--no-tty','--volume',f'{a.qemu.resolve().parent}:/emu','--volume',f'{a.pack.resolve()}:/pack','--volume',f'{a.share.resolve()}:/share',str(a.wasm.resolve()),'--','/emu/'+a.qemu.name,*args]
start=time.monotonic();report={};log=(out/'exec-http.log').open('wb');p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
def request(path,method='GET',body=None,headers={}):
 r=urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:18081'+path,method=method,data=body,headers=headers),timeout=5)
 return r.status,json.load(r)
def submit(source,stdin=b''):
 source=source.encode();return request('/run','POST',source+stdin,{'X-Source-Length':str(len(source))})[1]['id']
def result(i,limit=180):
 end=time.monotonic()+limit
 while time.monotonic()<end:
  v=request('/jobs/'+str(i))[1]
  if v['state']>=3 and v['reaped']:
   v['output']=base64.b64decode(v.pop('output_base64')).decode(errors='replace');return v
  if p.poll() is not None:raise RuntimeError('supervisor exited')
  time.sleep(.25)
 raise RuntimeError('deadline')
try:
 for _ in range(100):
  try:request('/health');break
  except urllib.error.URLError:time.sleep(.1)
 report['health_seconds']=time.monotonic()-start
 i=submit('print("USER_LUA",io.read("*a")); print(os.execute("id -u; id -G")); assert(io.open("/dev/tty", "w")==nil); assert(io.open("/dev/console", "w")==nil); assert(io.open("/dev/ttyS0", "w")==nil); assert(io.open("/proc/1/fd/1", "w")==nil); assert(io.open("/tmp/request.output", "w")==nil); local f=assert(io.open("/work/canary", "w")); f:write("first"); f:close(); print("EXEC_END_MARK:0")',b'input works')
 report['first']=result(i);assert report['first']['state']==3 and report['first']['exit_code']==0 and 'USER_LUA' in report['first']['output'] and '65534' in report['first']['output'],report['first']
 i=submit('assert(io.open("/work/canary")==nil); print("FRESH_REQUEST_PASS"); error("EXPECTED_LUA_ERROR")')
 report['second']=result(i);assert report['second']['state']==3 and report['second']['exit_code']!=0 and 'FRESH_REQUEST_PASS' in report['second']['output'],report['second']
 i=submit('while true do end')
 end=time.monotonic()+180
 while time.monotonic()<end:
  if request('/jobs/'+str(i))[1]['state']==2:break
  time.sleep(.25)
 else:raise AssertionError('cancel case never reached executing state')
 request('/jobs/'+str(i)+'/cancel','POST');report['cancel']=result(i,10);assert report['cancel']['state']==4
 report['result']='passed'
except Exception as e:report['result']='failed';report['error']=str(e)
finally:
 try:os.killpg(p.pid,signal.SIGKILL)
 except ProcessLookupError:pass
 p.wait();log.close();report['elapsed_seconds']=time.monotonic()-start;report['host_peak_rss_kib']=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
 (out/'exec-http-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='passed' else 1)
