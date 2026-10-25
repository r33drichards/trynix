#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <wasix/context.h>
static int shared;
static wasix_context_id_t child, main_context;
static void *thread(void *unused) { shared = 42; return 0; }
static void context(void) { shared = 99; if (wasix_context_switch(main_context)) abort(); abort(); }
int main(int argc, char **argv) {
  if (shared != 0) return 10;
  FILE *f = fopen("/tmp/trynix-isolation-marker", "r");
  if (f) { fclose(f); return 11; }
  f = fopen("/tmp/trynix-isolation-marker", "w");
  if (!f) return 12;
  fputs("private per-instance marker", f); fclose(f);
  puts("fresh-instance: PASS");
  if (argc == 2 && strcmp(argv[1], "loop") == 0) {
    puts("loop: READY"); fflush(stdout); for (;;) {}
  }
  pthread_t t; int p[2]; char c;
  if (pthread_create(&t,0,thread,0) || pthread_join(t,0) || shared != 42) return 1;
  puts("pthread: PASS");
  if(pipe(p) || write(p[1],"x",1)!=1 || read(p[0],&c,1)!=1 || c!='x') return 2;
  close(p[0]);close(p[1]);puts("pipe: PASS");
  main_context = wasix_context_main;
  if(wasix_context_create(&child,context)) {perror("context_create");return 3;}
  if(wasix_context_switch(child) || shared!=99 || wasix_context_destroy(child)) return 4;
  puts("context: PASS");
  printf("memory_pages: %zu\n", (size_t)__builtin_wasm_memory_size(0));
  return 0;
}
