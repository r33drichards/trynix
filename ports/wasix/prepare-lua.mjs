// Fetch a bounded official Nix closure, verify signatures and NAR hashes,
// then unpack into a NEW isolated share. Never execute package contents here.
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { zstdDecompressSync } from 'node:zlib';
import { webcrypto } from 'node:crypto';
import { parseNarinfo } from '../../site/js/closure.js';
import { verify, DEFAULT_SUBSTITUTERS } from '../../site/js/substituters.js';
import { verifyHash } from '../../site/js/hash.js';
import { parseNar } from '../../site/js/nar.js';
globalThis.crypto ??= webcrypto;
const root = '1xi0ly6fi0mw561qmv2mf3r20v17frl9-lua-5.5.1';
const dest = path.resolve(process.argv[2] || 'lua-share');
if (fs.existsSync(dest)) throw Error('Destination must not exist');
const cache = 'https://cache.nixos.org/';
function fetch(url, limit) {
 return execFileSync('curl', ['-fLsS', '--max-time', '90', '--max-filesize', String(limit), url], {maxBuffer:limit});
}
const infos = new Map(), pending = [root.slice(0,32)];
let total = 0;
while (pending.length) {
 const digest=pending.pop(); if(infos.has(digest)) continue;
 if(!/^[0-9a-z]{32}$/.test(digest) || infos.size>=32) throw Error('Invalid/oversized closure');
 const info=parseNarinfo(fetch(cache+digest+'.narinfo',65536).toString());
 if(await verify(info,DEFAULT_SUBSTITUTERS)!==true) throw Error('Invalid signature');
 if(!info.storePath.startsWith('/nix/store/'+digest+'-') || !/^\/nix\/store\/[a-z0-9]{32}-[^/]+$/.test(info.storePath)) throw Error('Invalid store path');
 total+=info.narSize; if(!Number.isSafeInteger(info.narSize)||info.narSize<=0||total>128*1024**2) throw Error('NAR budget exceeded');
 infos.set(digest,info); pending.push(...info.references.map(x=>x.slice(0,32)));
}
fs.mkdirSync(dest,{recursive:false}); fs.mkdirSync(path.join(dest,'nix/store'),{recursive:true});
for(const info of infos.values()) {
 const url=new URL(info.url,cache); if(url.origin!==new URL(cache).origin) throw Error('Unexpected archive origin');
 const packed=fetch(url.href,64*1024**2);
 let nar;
 if(info.compression==='xz') nar=execFileSync('xz',['-dc'],{input:packed,maxBuffer:info.narSize+1});
 else if(info.compression==='bzip2') nar=execFileSync('bzip2',['-dc'],{input:packed,maxBuffer:info.narSize+1});
 else if(info.compression==='zstd') nar=zstdDecompressSync(packed,{maxOutputLength:info.narSize+1});
 else if(info.compression==='none') nar=packed;
 else throw Error('Unsupported compression '+info.compression);
 if(nar.length!==info.narSize || !(await verifyHash(nar,info.narHash))) throw Error('Invalid NAR bytes');
 const base=path.join(dest,info.storePath.slice(1));
 for(const e of parseNar(nar)) {
  if(e.path.startsWith('/') || e.path.split('/').some(x=>x==='..'||x==='.')) throw Error('Unsafe NAR path');
  const target=path.join(base,e.path);
  for(let p=path.dirname(target); p!==dest; p=path.dirname(p)) {
   if(p===path.dirname(p)) throw Error('Escaped destination');
   if(fs.existsSync(p)&&fs.lstatSync(p).isSymbolicLink()) throw Error('Symlink parent');
  }
  if(e.type==='directory') fs.mkdirSync(target);
  else if(e.type==='regular') fs.writeFileSync(target,e.data,{flag:'wx',mode:e.executable?0o755:0o644});
  else if(e.type==='symlink') fs.symlinkSync(e.target,target);
 }
 console.log('verified and unpacked',info.storePath);
}
fs.mkdirSync(path.join(dest,'bin'));fs.symlinkSync('/nix/store/'+root+'/bin/lua',path.join(dest,'bin/lua'));
fs.writeFileSync(path.join(dest,'manifest'),`export PATH=/share/bin:/nix/store/${root}/bin:/bin\n`);
fs.writeFileSync(path.join(dest,'closure-evidence.json'),JSON.stringify({root,paths:[...infos.values()],narBytes:total},null,2));
console.log('Lua share ready; no guest execution performed',dest);
