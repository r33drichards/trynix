#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/statvfs.h>
static int linux_errno(int err) {
#include "9p-errno-wasix.inc"
}
int main(void) {
 if(linux_errno(ENOENT)!=2 || linux_errno(EACCES)!=13 || linux_errno(ENOTSUP)!=95) return 1;
 puts("errno translation: PASS");
 DIR *d=opendir("/"); if(!d) return 2;
 struct dirent *a=readdir(d); if(!a) return 3;
 long p=telldir(d); printf("directory cookie: %ld\n",p); if(p<0) return 4;
 struct dirent *b=readdir(d); if(!b) return 5;
 char name[1024]; snprintf(name,sizeof name,"%s",b->d_name);
 seekdir(d,p); b=readdir(d); if(!b || strcmp(name,b->d_name)) return 6;
 puts("directory seek/replay: PASS"); closedir(d);
 struct statvfs v; int rc=statvfs("/",&v); printf("statvfs: rc=%d errno=%d blocksize=%lu\n",rc,errno,rc?0:v.f_bsize);
 return 0;
}
