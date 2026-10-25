#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
static jmp_buf env;
__attribute__((noinline)) static void jump(void) { longjmp(env, 7); }
int main(int argc, char **argv) {
    const unsigned count = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000;
    for (volatile unsigned i = 0; i < count; ++i) {
        int v = setjmp(env);
        if (!v) jump();
        if (v != 7) return 1;
        if (i % 100000 == 0) {
            printf("jumps=%u memory_pages=%lu\n", i,
                   (unsigned long)__builtin_wasm_memory_size(0));
            fflush(stdout);
        }
    }
    return 0;
}
