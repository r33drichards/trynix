#!/usr/bin/env bash
# Usage: test-generated-thunks.sh PATCHED_QEMU DEPS OUTPUT_DIRECTORY
set -euo pipefail
[[ $# == 3 ]] || { echo "usage: $0 PATCHED_QEMU DEPS OUTPUT_DIRECTORY" >&2; exit 2; }
QEMU=$(realpath "$1"); DEPS=$(realpath "$2"); mkdir -p "$3"; OUT=$(realpath "$3")
PORT=$(cd "$(dirname "$0")" && pwd)
python3 - "$QEMU/accel/tcg/tcg-runtime.c" "$OUT/memset-adapter.inc" <<'PY'
from pathlib import Path
import re,sys
s=Path(sys.argv[1]).read_text()
m=re.findall(r'static void \*wasix_helper_memset\(void \*dest, int c, void \*len\)\n\{\n    return memset\(dest, c, \(uintptr_t\)len\);\n\}',s)
if len(m)!=1: raise SystemExit('Memset adapter changed; review test extraction')
Path(sys.argv[2]).write_text(m[0]+'\n')
PY
wasixcc -O2 -pthread -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
 -I"$QEMU/include" -I"$DEPS/include" -I"$OUT" "$PORT/test-generated-thunks.c" \
 "$DEPS/lib/libffi.a" -o "$OUT/thunks.wasm"
"${WASM_DIS:-$HOME/.wasixcc/binaryen/bin/wasm-dis}" "$OUT/thunks.wasm" -o "$OUT/thunks.wat"
python3 "$PORT/cache-longjmp.py" "$OUT/thunks.wat" "$OUT/thunks.cached.wat"
"${WASM_AS:-$HOME/.wasixcc/binaryen/bin/wasm-as}" "$OUT/thunks.cached.wat" -o "$OUT/thunks.cached.wasm" \
 --enable-exception-handling --enable-reference-types --enable-bulk-memory \
 --enable-simd --enable-relaxed-simd --enable-extended-const --enable-threads \
 --enable-mutable-globals --enable-sign-ext --enable-nontrapping-float-to-int --enable-multivalue
wasmer run --offline "$OUT/thunks.wasm" -- 1000
wasmer run --offline "$OUT/thunks.cached.wasm" -- 1000000
