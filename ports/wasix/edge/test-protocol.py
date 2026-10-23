"""Small fake-guest HTTP regressions; not Linux execution evidence."""
import base64,json,os,signal,socket,subprocess,time,urllib.request,urllib.error
from pathlib import Path
import argparse
parser=argparse.ArgumentParser();parser.add_argument('wasm',type=Path);parser.add_argument('fake',type=Path);parser.add_argument('out',type=Path);a=parser.parse_args();a.out.mkdir();root=a.out
cmd=['wasmer','run','--offline','--net','--no-tty','--volume',f'{a.fake.resolve().parent}:/probe',str(a.wasm.resolve()),'--','/probe/'+a.fake.name]
p=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
def raw(parts):
 s=socket.create_connection(('127.0.0.1',18081),timeout=5)
 for part in parts:s.sendall(part);time.sleep(.02)
 s.shutdown(socket.SHUT_WR);data=b''
 while True:
  chunk=s.recv(65536)
  if not chunk:break
  data+=chunk
 s.close();return int(data.split(b' ')[1]),data.split(b'\r\n\r\n',1)[1]
def req(path,method='GET',body=None,headers={}):
 with urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:18081'+path,method=method,data=body,headers=headers),timeout=5) as r:return r.status,json.load(r)
def finish(i):
 for _ in range(100):
  r=req('/jobs/'+str(i))[1]
  if r['state']>=3 and r['reaped']:return r
  time.sleep(.05)
 raise AssertionError('job did not finish')
results=[]
try:
 for _ in range(100):
  try:req('/health');break
  except urllib.error.URLError:time.sleep(.05)
 cases=[(b'POST /run HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\nX-Source-Length: 1\r\n\r\nx',400),
 (b'POST /run HTTP/1.1\r\nContent-Length: 1\r\nX-Source-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\nx',400),
 (b'POST /run HTTP/1.1\r\nContent-Length: 999999\r\nX-Source-Length: 1\r\n\r\n',413),
 (b'POST /run HTTP/1.1\r\nContent-Length: 20\r\nX-Source-Length: 1\r\n\r\nx',400),
 (b'GET /health HTTP/1.1\r\nX-Fill: '+b'a'*8200+b'\r\n\r\n',400)]
 for request,expected in cases:
  got,_=raw([request]);assert got==expected,(got,expected);results.append(got)
 got,body=raw([b'POST /run HTTP/1.1\r\nContent-Length: 4\r\n',b'X-Source-Length: 4\r\n\r\nPA',b'SS']);assert got==202
 i=json.loads(body)['id'];r=finish(i);assert r['state']==3 and r['exit_code']==7,r;assert base64.b64decode(r['output_base64'])==b'fake\x00bytes',r
 for path in [f'/jobs/{i}/nonsense',f'/jobs/{i}/cancelled']:
  got,_=raw([f'POST {path} HTTP/1.1\r\nContent-Length: 0\r\n\r\n'.encode()]);assert got==404,(path,got)
 i=req('/run','POST',b'BUSY',{'X-Source-Length':'4'})[1]['id'];time.sleep(.1)
 got,_=raw([b'POST /run HTTP/1.1\r\nContent-Length: 4\r\nX-Source-Length: 4\r\n\r\nPASS']);assert got==429
 req(f'/jobs/{i}/cancel','POST');assert finish(i)['state']==4
 i=req('/run','POST',b'PASS',{'X-Source-Length':'4'})[1]['id'];assert finish(i)['exit_code']==7
 print(json.dumps({'result':'passed','malformed_statuses':results,'fragmented_request':True,'split_exit_record':7,'binary_output':True,'exact_cancel_route':True,'busy_rejected':True,'cancel_then_fresh':True},indent=2))
finally:
 try:os.killpg(p.pid,signal.SIGKILL)
 except ProcessLookupError:pass
 output=p.communicate(timeout=5)[0];(root/'protocol.log').write_bytes(output)
