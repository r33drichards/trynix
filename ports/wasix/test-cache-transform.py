#!/usr/bin/env python3
"""Fail-closed text-shape regression; execution ABI is tested separately."""
from pathlib import Path
import subprocess,sys,tempfile,unittest
TOOL=Path(__file__).with_name('cache-longjmp.py')
BASE='''(module
(import "wasi_snapshot_preview1" "proc_exit" (func $exit (param i32)))
(tag $tag (type $type) (param i32))
(export "__c_longjmp" (tag $tag))
(func $jump (param $0 i32) (throw $tag (i32.add (local.get $0) (i32.const 8))))
)'''
class CacheTransformTests(unittest.TestCase):
 def transform(self,text):
  with tempfile.TemporaryDirectory() as td:
   a=Path(td)/'in.wat';b=Path(td)/'out.wat';a.write_text(text)
   r=subprocess.run([sys.executable,'-O',str(TOOL),str(a),str(b)],capture_output=True)
   return r.returncode,b.read_text() if b.exists() else ''
 def test_expected_shape(self):
  rc,s=self.transform(BASE);self.assertEqual(rc,0);self.assertIn('(global $key63',s);self.assertIn('(call $exit (i32.const 78))',s);self.assertIn('(throw_ref (global.get $cache0))',s)
 def test_reject_changed_shapes(self):
  cases=[BASE.replace('(param i32))\n(export','(param i64))\n(export'),BASE.replace('(i32.const 8)','(i32.const 4)'),BASE.replace('(export "__c_longjmp" (tag $tag))',''),BASE.replace('\n)', '\n(func $extra (throw $tag (i32.const 1)))\n)'),BASE+'\n;; $port_cached_longjmp\n']
  for s in cases:
   with self.subTest(s=s):rc,out=self.transform(s);self.assertNotEqual(rc,0);self.assertEqual(out,'')
if __name__=='__main__':unittest.main()
