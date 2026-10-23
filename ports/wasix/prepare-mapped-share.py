from pathlib import Path
import os,stat,shutil,sys
if len(sys.argv) != 3:
 raise SystemExit('usage: prepare-mapped-share.py SOURCE NEW_DESTINATION')
src=Path(sys.argv[1]).resolve();dst=Path(sys.argv[2]).resolve()
if not src.is_dir(): raise SystemExit('Source must be a directory')
if dst == src or src in dst.parents:
 raise SystemExit('Destination must be outside source tree')
dst.mkdir()
count=0
def metadata(directory,name,mode):
 global count
 md=directory/'.virtfs_metadata';md.mkdir(exist_ok=True)
 (md/name).write_text(f'virtfs.uid=0\nvirtfs.gid=0\nvirtfs.mode={mode}\nvirtfs.rdev=0\n');count+=1
def copy(a,b):
 st=a.lstat();mode=st.st_mode
 if stat.S_ISDIR(mode):
  b.mkdir(exist_ok=True)
  (b/'.virtfs_metadata_root').write_text(f'virtfs.uid=0\nvirtfs.gid=0\nvirtfs.mode={mode}\nvirtfs.rdev=0\n')
  for p in a.iterdir():
   if p.name.startswith('.virtfs_metadata'):raise RuntimeError('Reserved metadata name')
   copy(p,b/p.name);metadata(b,p.name,p.lstat().st_mode)
 elif stat.S_ISREG(mode):shutil.copyfile(a,b)
 elif stat.S_ISLNK(mode):b.write_text(os.readlink(a))
 else:raise RuntimeError('Unsupported source file type')
copy(src,dst)
print('mapped entries',count)
