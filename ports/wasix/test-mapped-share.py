#!/usr/bin/env python3
import json,os,stat,subprocess,sys,tempfile,unittest
from pathlib import Path
SCRIPT=Path(__file__).with_name('prepare-mapped-share.py')
class MappedShareTests(unittest.TestCase):
 def test_modes_links_and_nested_rejection(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);src=root/'source';src.mkdir();(src/'program').write_text('payload');(src/'program').chmod(0o755);(src/'data').write_text('data');(src/'data').chmod(0o644);(src/'link').symlink_to('/nix/store/example/program')
   bad=subprocess.run([sys.executable,str(SCRIPT),str(src),str(src/'nested')],capture_output=True)
   self.assertNotEqual(bad.returncode,0);self.assertFalse((src/'nested').exists())
   dst=root/'mapped';subprocess.run([sys.executable,str(SCRIPT),str(src),str(dst)],check=True,capture_output=True)
   def mode(name):return int(dict(x.split('=',1) for x in (dst/'.virtfs_metadata'/name).read_text().splitlines())['virtfs.mode'])
   self.assertEqual(mode('program'),stat.S_IFREG|0o755);self.assertEqual(mode('data'),stat.S_IFREG|0o644)
   self.assertTrue(stat.S_ISLNK(mode('link')));self.assertFalse((dst/'link').is_symlink());self.assertEqual((dst/'link').read_text(),'/nix/store/example/program');self.assertEqual((dst/'program').read_text(),'payload')
if __name__=='__main__':unittest.main()
