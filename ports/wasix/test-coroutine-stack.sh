#!/usr/bin/env bash
# Build and run the coroutine C-stack isolation test against the real backend.
# Requires wasixcc (or an equivalent WASIX clang + sysroot) and Wasmer.
# Override CC/SYSROOT/WASMER/LLD if the pinned toolchain is not on PATH.
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
CC=${CC:-wasixcc}
WASMER=${WASMER:-wasmer}
N=${1:-512}
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
# The test includes qemu/osdep.h and qemu/coroutine_int.h; provide the same
# minimal stand-ins the port's own ABI harness uses.
"$CC" -O2 -pthread -I"$PORT/test-stubs" -c "$PORT/test-coroutine-stack.c" -o "$OUT/test.o"
"$CC" -O2 -pthread -I"$PORT/test-stubs" -c "$PORT/coroutine-wasix.c" -o "$OUT/backend.o"
"$CC" -O2 -c "$PORT/coroutine-wasix-switch.S" -o "$OUT/shim.o"
"$CC" "$OUT/test.o" "$OUT/backend.o" "$OUT/shim.o" -o "$OUT/test.wasm"
"$WASMER" run "$OUT/test.wasm" -- "$N"
