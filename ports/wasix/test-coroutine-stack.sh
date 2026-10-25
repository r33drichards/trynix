#!/usr/bin/env bash
# Coroutine C-stack isolation gate for the WASIX coroutine backend.
#
# Compiles ports/wasix/test-coroutine-stack.c against two backend variants and
# requires the expected outcome from each:
#   fixed   (per-coroutine C stack + coroutine-wasix-switch.S) -> must pass
#   pre-fix (shared guest C stack, no switching)               -> must trap
# The pre-fix variant is derived from the real backend in a temp dir, so the
# gate demonstrates the difference instead of asserting it.
#
# Stub qemu/*.h headers are generated here so the backend can be compiled
# outside a QEMU source tree.
# Usage: bash ports/wasix/test-coroutine-stack.sh [N_PASS] [N_TRAP]
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
N_PASS=${1:-1500}
N_TRAP=${2:-512}
CC=${CC:-wasixcc}
WASMER=${WASMER:-wasmer}
T=$(mktemp -d)
trap "rm -rf \"$T\"" EXIT
mkdir -p "$T/stubs/qemu" "$T/prefix"
cat > "$T/stubs/qemu/osdep.h" <<"STUB_OSDEP"
#ifndef TEST_QEMU_OSDEP_H
#define TEST_QEMU_OSDEP_H
/* Minimal stand-in for qemu/osdep.h so the real coroutine backend can be
 * compiled and exercised outside a full QEMU build. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof(((type *)0)->member) *_mptr = (ptr);       \
        (type *)((char *)_mptr - offsetof(type, member)); })
#endif
#ifndef offsetof
#define offsetof(type, field) __builtin_offsetof(type, field)
#endif
#ifndef ROUND_UP
#define ROUND_UP(n, d) (((uintptr_t)(n) + ((uintptr_t)(d) - 1)) & ~((uintptr_t)(d) - 1))
#endif
#define g_new0(type, count) ((type *)memset(malloc(sizeof(type) * (count)), 0, sizeof(type) * (count)))
#define g_malloc0(size) calloc(1, (size))
#define g_free free
#endif
STUB_OSDEP
cat > "$T/stubs/qemu/coroutine_int.h" <<"STUB_CINT"
#ifndef TEST_COROUTINE_INT_H
#define TEST_COROUTINE_INT_H
#include "qemu/osdep.h"
/* Mirrors the fields of include/qemu/coroutine-core.h used by the backend. */
typedef struct Coroutine Coroutine;
typedef enum { COROUTINE_SUSPENDED, COROUTINE_YIELD, COROUTINE_TERMINATE } CoroutineAction;
struct Coroutine {
    Coroutine *caller;
    void (*entry)(void *);
    void *entry_arg;
};
Coroutine *qemu_coroutine_new(void);
void qemu_coroutine_delete(Coroutine *co);
CoroutineAction qemu_coroutine_switch(Coroutine *from, Coroutine *to, CoroutineAction action);
Coroutine *qemu_coroutine_self(void);
bool qemu_in_coroutine(void);
#endif
STUB_CINT
cat > "$T/stubs/qemu/coroutine-tls.h" <<"STUB_TLS"
#ifndef TEST_COROUTINE_TLS_H
#define TEST_COROUTINE_TLS_H
/* Stand-in for qemu/coroutine-tls.h. The backend expects
 * QEMU_DEFINE_STATIC_CO_TLS(type, name) to provide get_<name>() /
 * set_<name>(); single-threaded tests can use thread-local storage. */
#define QEMU_DEFINE_STATIC_CO_TLS(type, name)         \
    static _Thread_local type name;                   \
    static type get_##name(void) { return name; }      \
    static void set_##name(type val) { name = val; }
#endif
STUB_TLS
compile(){ "$CC" -O2 -pthread -I"$T/stubs" -c "$1" -o "$2"; }
echo "== build test + fixed backend + shim =="
compile "$PORT/test-coroutine-stack.c" "$T/test.o"
compile "$PORT/coroutine-wasix.c" "$T/backend-fixed.o"
compile "$PORT/coroutine-wasix-switch.S" "$T/shim.o"
"$CC" "$T/test.o" "$T/backend-fixed.o" "$T/shim.o" -o "$T/fixed.wasm"
set +e
out=$("$WASMER" run "$T/fixed.wasm" -- "$N_PASS" 2>&1); rc=$?
set -e
echo "fixed:    n=$N_PASS rc=$rc $out"
if [ "$rc" -ne 0 ] || ! printf "%s" "$out" | grep -q "\"bad_guard_bytes\":0"; then
  echo "FAIL: the fixed backend must pass with zero bad guard bytes"
  exit 1
fi
echo "== derive pre-fix backend (shared guest C stack) =="
sed -e "s/wasix_sp_ctx_switch(&from->sp, to->sp, to->context)/wasix_context_switch(to->context)/" \
    -e "s/co->sp = co->stack_top = (char \*)co->stack + COROUTINE_WASIX_STACK_SIZE;/co->sp = wasix_sp_get();/" \
    -e "s/co->sp = (void \*)ROUND_UP((uintptr_t)co->sp, COROUTINE_WASIX_STACK_ALIGN);//" \
    "$PORT/coroutine-wasix.c" > "$T/prefix/coroutine-wasix.c"
grep -q "wasix_context_switch(to->context)" "$T/prefix/coroutine-wasix.c" || { echo "FAIL: could not derive the pre-fix variant"; exit 1; }
compile "$T/prefix/coroutine-wasix.c" "$T/backend-prefix.o"
"$CC" "$T/test.o" "$T/backend-prefix.o" "$T/shim.o" -o "$T/prefix.wasm"
set +e
out2=$("$WASMER" run "$T/prefix.wasm" -- "$N_TRAP" 2>&1); rc2=$?
set -e
echo "pre-fix:  n=$N_TRAP rc=$rc2 $(printf "%s" "$out2" | head -2 | tr "\n" " ")"
if [ "$rc2" -eq 0 ]; then
  echo "FAIL: the shared-stack backend survived N=$N_TRAP, so this gate proves nothing"
  exit 1
fi
echo "::notice::gate-fixed n=$N_PASS rc=$rc $(printf "%s" "$out" | tr '"' ' ')"'
echo "::notice::gate-shared-stack n=$N_TRAP rc=$rc2 $(printf "%s" "$out2" | head -2 | tr '"' ' ')"'
echo "GATE OK: fixed backend passes at N=$N_PASS; shared-stack backend fails at N=$N_TRAP"
