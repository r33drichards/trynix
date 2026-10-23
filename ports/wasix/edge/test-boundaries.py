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
   v['output']=base64.b64decode(v.pop('output_base64')).decode(errors='replace');return v
  if p.poll() is not None:raise RuntimeError('supervisor exited')
  time.sleep(.25)
 raise RuntimeError('deadline')
try:
 for _ in range(100):
  try:request('/health');break
  except urllib.error.URLError:time.sleep(.1)
 report['health_seconds']=time.monotonic()-start
 source='local s=io.read("*a"); assert(#s==65536); io.write("MAX_INPUT_PASS",string.char(0)); --' 
 source += 'x'*(65536-len(source))
 i=submit(source,b'z'*65536);report['max_input']=result(i);assert report['max_input']['state']==3 and report['max_input']['exit_code']==0 and 'MAX_INPUT_PASS' in report['max_input']['output'],report['max_input']
 i=submit('io.write(string.rep("X",200000)); io.flush()');report['output_limit']=result(i);assert report['output_limit']['reaped'] and len(report['output_limit']['output'])<=131072,report['output_limit']
 # Keep large bounded output out of the committed report.
 report['output_limit']['output_bytes']=len(report['output_limit'].pop('output'))
 i=submit('while true do end');report['timeout']=result(i);assert report['timeout']['state']==5 and report['timeout']['error']=='execution timeout',report['timeout']
 report['result']='passed'
except Exception as e:report['result']='failed';report['error']=str(e)
finally:
 try:os.killpg(p.pid,signal.SIGKILL)
 except ProcessLookupError:pass
 p.wait();log.close();report['elapsed_seconds']=time.monotonic()-start;report['host_peak_rss_kib']=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
 (out/'boundary-http-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='passed' else 1)
