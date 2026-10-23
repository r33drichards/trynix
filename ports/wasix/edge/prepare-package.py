#!/usr/bin/env python3
"""Prepare a local-only Wasmer package manifest; never uploads or deploys."""
import argparse, json, os, shutil
from pathlib import Path
p=argparse.ArgumentParser()
for name in ('supervisor','qemu','pack','share','out'):p.add_argument(name,type=Path)
p.add_argument('--package-name',default='local/trynix-lua-poc')
a=p.parse_args()
for path in (a.supervisor,a.qemu,a.pack/'machine.json'):
 if not path.is_file():p.error(f'missing input: {path}')
if not a.share.is_dir():p.error('missing mapped share')
a.out.mkdir();(a.out/'emu').mkdir()
for src,dst in [(a.supervisor,a.out/'exec-api.wasm'),(a.qemu,a.out/'emu/qemu-system-x86_64.wasm')]:
 try:os.link(src,dst)
 except OSError:shutil.copyfile(src,dst)
args=['/emu/qemu-system-x86_64.wasm']+[x.replace('{pack}','/pack').replace('{share}','/share').replace('{ram}','512M').replace('tcg,tb-size=500','tcg,tb-size=16').replace('security_model=none','security_model=mapped-file,readonly=on') for x in json.loads((a.pack/'machine.json').read_text())['args']]
manifest='[package]\nname = '+json.dumps(a.package_name)+'\nversion = "0.0.1"\ndescription = "Experimental QEMU Linux Lua POC"\nentrypoint = "serve"\n\n[[module]]\nname = "supervisor"\nsource = "exec-api.wasm"\n\n[[command]]\nname = "serve"\nmodule = "supervisor"\nrunner = "wasi"\n\n[command.annotations.wasi]\nmain-args = '+json.dumps(args)+'\nenv = ["PUBLIC_POC=1", "PORT=80"]\n\n[fs]\n"/emu" = "emu"\n"/pack" = '+json.dumps(str(a.pack.resolve()))+'\n"/share" = '+json.dumps(str(a.share.resolve()))+'\n'
(a.out/'wasmer.toml').write_text(manifest)
(a.out/'NOT-FOR-PUBLISH.txt').write_text('Local packaging only. Confirm expiry, free-quota exposure, Edge compatibility and complete redistribution source/license obligations before uploading. No token, snapshot or persistent volume configured.\n')
print('Prepared local manifest; build with wasmer package build. No upload or deployment performed.')
