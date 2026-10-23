#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(void) {
  static char line[200000];
  puts("trynix: waiting for the store"); fflush(stdout);
  if (!fgets(line,sizeof line,stdin)) return 2;
  puts("trynix: welcome to the multiverse"); fflush(stdout);
  if (!fgets(line,sizeof line,stdin)) return 3;
  puts("INPUT_READY_MARK"); fflush(stdout);
  if (!fgets(line,sizeof line,stdin)) return 4;
  puts("EXEC_RUNNING_MARK"); fflush(stdout);
  if (strstr(line,"QlVTWQ==")) {
    /* Test-only evidence that BUSY was admitted and reached the child. */
    FILE *marker = fopen("/probe/fake-busy-entered", "wb");
    if (!marker) return 5;
    int failed = fputs("busy-entered\n", marker) == EOF;
    if (fclose(marker) || failed) return 6;
    for (;;) sleep(1);
  }
  fputs("\nEXEC_BEGIN_MARK\nZmFrZQBieXRlcw==\n\nEXEC_END_MARK:",stdout); fflush(stdout);
  usleep(200000); puts("7"); fflush(stdout);
  for (;;) sleep(1);
}
