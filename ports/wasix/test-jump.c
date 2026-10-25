#include <stdio.h>
#include <setjmp.h>
static sigjmp_buf env;
__attribute__((noinline)) static void jump(void) { siglongjmp(env,7); }
int main(void) { int v=sigsetjmp(env,0); if(!v)jump(); printf("jump=%d\n",v);return v==7?0:1; }
