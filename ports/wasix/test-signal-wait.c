#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
static void *sender(void *x){(void)x;usleep(10000);int r=kill(getpid(),SIGUSR1);printf("kill=%d errno=%d\n",r,errno);return 0;}
int main(void){sigset_t set;sigemptyset(&set);sigaddset(&set,SIGUSR1);printf("block=%d\n",pthread_sigmask(SIG_BLOCK,&set,0));pthread_t t;pthread_create(&t,0,sender,0);int sig=0x55555555;int r=sigwait(&set,&sig);printf("wait=%d sig=%d errno=%d\n",r,sig,errno);pthread_join(t,0);return r||sig!=SIGUSR1;}
