"""Local package Linux/Lua smoke with no expiry; not Edge evidence."""
import base64,json,os,resource,signal,subprocess,time,urllib.request,urllib.error
from pathlib import Path
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('package',type=Path)
parser.add_argument('out',type=Path)
a=parser.parse_args();out=a.out.resolve();out.mkdir()
cmd=['wasmer','run','--offline','--net','--no-tty','--env','PORT=18082',str(a.package.resolve())]
start=time.monotonic();report={};log=(out/'package.log').open('wb');p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
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
 (out/'package-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
raise SystemExit(0 if report['result']=='passed' else 1)
