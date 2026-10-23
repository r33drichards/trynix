import argparse,base64,json,os,resource,signal,subprocess,time,urllib.request,urllib.error
from pathlib import Path
parser=argparse.ArgumentParser()
for name in ('wasm','qemu','pack','share','out'):parser.add_argument(name,type=Path)
a=parser.parse_args();out=a.out;out.mkdir()
machine=json.loads((a.pack/'machine.json').read_text())
args=[x.replace('{pack}','/pack').replace('{share}','/share').replace('{ram}','512M').replace('tcg,tb-size=500','tcg,tb-size=16').replace('security_model=none','security_model=mapped-file,readonly=on') for x in machine['args']]
cmd=['wasmer','run','--offline','--net','--no-tty','--volume',f'{a.qemu.resolve().parent}:/emu','--volume',f'{a.pack.resolve()}:/pack','--volume',f'{a.share.resolve()}:/share',str(a.wasm.resolve()),'--','/emu/'+a.qemu.name,*args]
start=time.monotonic();report={};log=(out/'boundary-http.log').open('wb');p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
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
   v['output']=base64.b64decode(v.pop('output_base64'),validate=True).decode(errors='strict');return v
  if p.poll() is not None:raise RuntimeError('supervisor exited')
  time.sleep(.25)
 raise RuntimeError('deadline')
try:
 for _ in range(100):
  try:request('/health');break
  except urllib.error.URLError:time.sleep(.1)
 report['health_seconds']=time.monotonic()-start
 prefix='--[['
 suffix=']] local s=io.read("*a"); assert(#s==8192 and s==string.rep("z",8192)); io.write("MAX_INPUT_PASS",string.char(0))'
 source=prefix+'x'*(8192-len(prefix)-len(suffix))+suffix
 i=submit(source,b'z'*8192);report['max_input']=result(i);assert report['max_input']['state']==3 and report['max_input']['exit_code']==0 and report['max_input']['output']=='MAX_INPUT_PASS\x00',report['max_input']
 i=submit('io.write(string.rep("X",131072)); assert(io.flush())');report['exact_output']=result(i);assert report['exact_output']['state']==3 and report['exact_output']['exit_code']==0 and report['exact_output']['output']=='X'*131072, {k:v for k,v in report['exact_output'].items() if k!='output'}
 report['exact_output']['output_bytes']=len(report['exact_output'].pop('output'))
 i=submit('io.write(string.rep("X",200000)); io.flush()');report['output_limit']=result(i);assert report['output_limit']['state']==3 and report['output_limit']['exit_code']!=0 and report['output_limit']['output']=='X'*131072,{k:v for k,v in report['output_limit'].items() if k!='output'}
 # Keep large bounded output out of the committed report.
 report['output_limit']['output_bytes']=len(report['output_limit'].pop('output'))
 i=submit('while true do end')
 deadline=time.monotonic()+180
 while time.monotonic()<deadline:
  v=request('/jobs/'+str(i))[1]
  if v['state']==2:break
  assert v['state']<3,v
  time.sleep(.1)
 else:raise AssertionError('timeout case never reached executing state')
 running=time.monotonic();report['timeout']=result(i);report['timeout']['observed_running_seconds']=time.monotonic()-running
 assert report['timeout']['state']==5 and report['timeout']['error']=='execution timeout' and 28<=report['timeout']['observed_running_seconds']<=42,report['timeout']
 report['result']='passed'
except Exception as e:report['result']='failed';report['error']=str(e)
finally:
 try:os.killpg(p.pid,signal.SIGKILL)
 except ProcessLookupError:pass
 p.wait();log.close();report['elapsed_seconds']=time.monotonic()-start;report['host_peak_rss_kib']=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
 (out/'boundary-http-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='passed' else 1)
