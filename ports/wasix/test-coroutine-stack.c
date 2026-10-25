/* SPDX-License-Identifier: LGPL-2.0-or-later */
/*
 * test-coroutine-stack.c - QEMU coroutine-API test for the WASIX backend.
 *
 * N coroutines are entered in turn and each suspends with an address-taken
 * guard array on its own C stack; only after all N are suspended are they
 * resumed to verify the guard. This is the pattern QEMU relies on (many
 * concurrent block/9p/NBD jobs), and it is what exposes a backend that gives
 * every coroutine the same linear-memory C stack.
 *
 * Exit 0 = all guards intact, 1 = corruption, 2 = usage error. With the
 * unfixed backend a large N traps (out-of-bounds memory access) instead.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "qemu/osdep.h"
#include "qemu/coroutine_int.h"

#define GUARD_BYTES 256u
#define FRAME_BYTES (16u * 1024u)
#define MAX_CO 4096

static Coroutine *cos[MAX_CO];
static Coroutine *leader_co;
static unsigned char exp_guard[GUARD_BYTES];
static int checked, bad_guard_bytes;
static uintptr_t span_lo = (uintptr_t)-1, span_hi;

static void worker_entry(void *arg)
{
    volatile unsigned char frame[FRAME_BYTES];
    volatile unsigned char guard[GUARD_BYTES];
    int id = (int)(uintptr_t)arg;
    uintptr_t lo = (uintptr_t)frame;
    if (lo < span_lo) span_lo = lo;
    if (lo + FRAME_BYTES > span_hi) span_hi = lo + FRAME_BYTES;
    memset((void *)frame, id & 0xff, FRAME_BYTES);
    memcpy((void *)guard, exp_guard, GUARD_BYTES);
    /* Suspend: every other coroutine may now consume stack space. */
    qemu_coroutine_switch(qemu_coroutine_self(), leader_co, COROUTINE_YIELD);
    for (unsigned i = 0; i < GUARD_BYTES; i++) {
        if (guard[i] != exp_guard[i]) bad_guard_bytes++;
    }
    checked++;
    /* Returning makes the trampoline issue COROUTINE_TERMINATE back to caller. */
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 64;
    if (n < 1 || n > MAX_CO) { fputs("n out of range\n", stderr); return 2; }
    for (unsigned i = 0; i < GUARD_BYTES; i++) exp_guard[i] = (unsigned char)(0x3Cu ^ i);
    leader_co = qemu_coroutine_self();
    for (int i = 0; i < n; i++) {
        Coroutine *co = qemu_coroutine_new();
        co->caller = leader_co;
        co->entry = worker_entry;
        co->entry_arg = (void *)(uintptr_t)i;
        cos[i] = co;
    }
    for (int i = 0; i < n; i++) {
        qemu_coroutine_switch(leader_co, cos[i], COROUTINE_YIELD);
    }
    for (int i = 0; i < n; i++) {
        qemu_coroutine_switch(leader_co, cos[i], COROUTINE_YIELD);
    }
    printf("{\"n\":%d,\"checked\":%d,\"bad_guard_bytes\":%d,\"stack_span_bytes\":%lu,"
           "\"frame_bytes\":%u,\"expected_span_if_isolated\":%lu}\n",
           n, checked, bad_guard_bytes, (unsigned long)(span_hi - span_lo),
           (unsigned)FRAME_BYTES, (unsigned long)((unsigned)FRAME_BYTES * (unsigned)n));
    fflush(stdout);
    for (int i = 0; i < n; i++) qemu_coroutine_delete(cos[i]);
    return bad_guard_bytes ? 1 : 0;
}
