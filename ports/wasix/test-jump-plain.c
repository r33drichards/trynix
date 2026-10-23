#include <stdio.h>
#include <setjmp.h>
static sigjmp_buf env;
__attribute__((noinline)) static void jump(void) { longjmp(env,7); }
int main(void) { int v=setjmp(env); if(!v)jump(); printf("jump=%d\n",v);return v==7?0:1; }
