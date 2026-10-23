#!/usr/bin/env python3
"""Pinned C-longjmp-only bounded exnref cache; see README limitations.
Input/output are Binaryen WAT. Exactly64 immutable key/ref pairs per store.
No Wasmer store fork/snapshot/restore/transfer or concurrent shared-store use.
"""
from pathlib import Path
import re,sys
if len(sys.argv) != 3:
 raise SystemExit('usage: cache-longjmp.py INPUT.wat OUTPUT.wat')
s=Path(sys.argv[1]).read_text()
match=re.search(r'\(export "__c_longjmp" \(tag (\$[^)]+)\)\)',s)
if not match: raise SystemExit('Missing exported C-longjmp tag')
if '$port_cached_longjmp' in s: raise SystemExit('Cache already present')
tag=match[1]
p=r'\(throw '+re.escape(tag)+r'\s+(\(i32\.add\s+\(local.get (\$\d+)\)\s+\(i32.const 8\)\s*\))\s*\)'
if len(re.findall(r'\(throw '+re.escape(tag)+r'(?=\s|\))',s))!=1:
 raise SystemExit('Expected exactly one original C longjmp throw')
if not re.search(r'\(tag '+re.escape(tag)+r'\s+\(type \$[^)]+\)\s+\(param i32\)\s*\)',s):
 raise SystemExit('C longjmp tag must have exactly one i32 payload')
s,n=re.subn(p,lambda m:'(call $port_cached_longjmp '+m[1]+')',s)
if n!=1:raise SystemExit('Unrecognized C longjmp throw shape')
h='\n'.join(f'(global $cache{i} (mut exnref) (ref.null exn)) (global $key{i} (mut i32) (i32.const 0))' for i in range(64))
h+='(func $port_cached_longjmp (param $p i32)\n'
# Full fixed-size search, then allocate only in an empty slot. No ref locals.
for i in range(64):h+=f'(if (i32.eq (global.get $key{i}) (local.get $p)) (then (throw_ref (global.get $cache{i}))))\n'
for i in range(64):
 h+=f'''(if (ref.is_null (global.get $cache{i})) (then
 (global.set $cache{i} (block $capture{i} (result exnref)
  (try_table (catch_all_ref $capture{i}) (throw {tag} (local.get $p))) (unreachable)))
 (global.set $key{i} (local.get $p)) (throw_ref (global.get $cache{i}))))\n'''
exitfn=re.search(r'\(import "[^"]+" "(?:proc_exit2|proc_exit)" \(func (\$[^\s)]+)',s)
if not exitfn:raise SystemExit('No existing process exit import')
h+=f'(call {exitfn[1]} (i32.const 78)) (unreachable))\n';pos=s.rfind(')');Path(sys.argv[2]).write_text(s[:pos]+h+s[pos:])
