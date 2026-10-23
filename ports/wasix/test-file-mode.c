#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
int main(void){struct stat s;const char*p="/share/nix/store/1xi0ly6fi0mw561qmv2mf3r20v17frl9-lua-5.5.1/bin/lua";int r=stat(p,&s);printf("stat=%d errno=%d mode=%o size=%lld accessX=%d\n",r,errno,s.st_mode,(long long)s.st_size,access(p,X_OK));return r;}
