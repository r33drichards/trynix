#!/usr/bin/env bash
# Install the WASIX toolchain revisions recorded in ports/wasix/pins.json
# (observed_toolchain_dependencies) without any account or registry login:
#   - wasix-org/llvm-project 21.1.206 (bundles Wasmer 7.5.0)
#   - wasix-org/wasix-libc v2026-10-02.1 sysroot-exnref-eh
#   - WebAssembly/binaryen version_133
# Publishes wasixcc/wasix++/wasixar/wasixranlib on PATH and records provenance.
# Every failure path prints what it saw: silent exits made the first runs undiagnosable.
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
TOOLS=${TOOLS:-${RUNNER_TEMP:-/tmp/wasix-tools}/tools}
mkdir -p "$TOOLS/bin" "$TOOLS/dl"
: > "$TOOLS/provenance.txt"
fetch(){ local url=$1 name=$2
  if [ -s "$TOOLS/dl/$name" ]; then echo "cached $name"; return 0; fi
  echo "fetching $name"
  if ! curl -fsSL --retry 3 --proto '=https' -o "$TOOLS/dl/$name" "$url"; then
    echo "DOWNLOAD FAILED url=$url"; exit 1
  fi
  echo "$name sha256=$(sha256sum "$TOOLS/dl/$name" | cut -d' ' -f1)" >> "$TOOLS/provenance.txt"
}
fetch https://github.com/wasix-org/llvm-project/releases/download/21.1.206/LLVM-Linux-x86_64.tar.gz llvm.tar.gz
fetch https://github.com/wasix-org/wasix-libc/releases/download/v2026-10-02.1/sysroot-exnref-eh.tar.gz sysroot.tar.gz
fetch https://github.com/WebAssembly/binaryen/releases/download/version_133/binaryen-version_133-x86_64-linux.tar.gz binaryen.tar.gz
tar xzf "$TOOLS/dl/llvm.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/sysroot.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/binaryen.tar.gz" -C "$TOOLS"
locate(){ # locate <label> <find-args...> ; sets global FOUND, prints to stdout
  local label=$1; shift
  mapfile -t HITS < <(find "$TOOLS" "$@" 2>/dev/null | head -1)
  if [ "${#HITS[@]}" -eq 0 ]; then
    echo "NOT FOUND: $label"; echo "--- tree under $TOOLS (maxdepth 3) ---"; find "$TOOLS" -maxdepth 3 | head -40; exit 1
  fi
  FOUND="${HITS[0]}"; echo "found $label=$FOUND"
}
echo "== locating toolchain pieces under $TOOLS =="
locate clang -type f -name clang;            CLANG="$FOUND"
locate wasmer -type f -name wasmer;          WASMER="$FOUND"
locate wasm-dis -type f -name wasm-dis;      WASMDIS="$FOUND"
locate crt1-command.o -type f -name 'crt1-command.o'; CRT1="$FOUND"
locate compiler-rt -type f -name 'libclang_rt.builtins-wasm32.a'; BUILTINS="$FOUND"
LLVM=$(dirname "$CLANG"); BINEN=$(dirname "$WASMDIS")
SYSROOT=$(dirname "$(dirname "$CRT1")")
RESROOT=$(dirname "$(dirname "$LLVM")")/lib/clang
echo "== clang resource dir: $RESROOT ==
mapfile -t RESS < <(find "$RESROOT" -maxdepth 2 -type d -name lib | head -1)
if [ "${#RESS[@]}" -eq 0 ]; then echo "resource lib dir missing"; ls -l "$RESROOT" 2>&1 | head -10; exit 1; fi
RES="${RESS[0]}"
# The release tarball ships no wasm compiler-rt in clang's resource dir; the sysroot
# does. Link it where clang looks so plain "wasixcc x.c -o x.wasm" behaves.
for t in wasm32-unknown-wasixp1 wasm32-unknown-wasi-preview1-threads wasm32-unknown-wasi; do
  mkdir -p "$RES/lib/$t"; ln -sf "$BUILTINS" "$RES/lib/$t/libclang_rt.builtins.a"
done
cat > "$TOOLS/bin/wasixcc" <<EOF
#!/bin/sh
exec "$CLANG" --target=wasm32-wasixp1 --sysroot="$SYSROOT" -Wl,--import-memory -Wl,--export-table -Wl,--shared-memory -Wl,--no-check-features "$@"
EOF
cat > "$TOOLS/bin/wasix++" <<EOF
#!/bin/sh
exec "$LLVM/clang++" --target=wasm32-wasixp1 --sysroot="$SYSROOT" "$@"
EOF
printf '#!/bin/sh\nexec %s/llvm-ar "$@"\n' "$LLVM" > "$TOOLS/bin/wasixar"
printf '#!/bin/sh\nexec %s/llvm-ranlib "$@"\n' "$LLVM" > "$TOOLS/bin/wasixranlib"
chmod +x "$TOOLS"/bin/*
export PATH="$TOOLS/bin:$LLVM:$BINEN:$PATH"
"$CLANG" --version | head -2
"$WASMER" --version | head -1
echo "sysroot=$SYSROOT"
echo "resource=$RES"
cat "$TOOLS/provenance.txt"
if [ -n "${GITHUB_PATH:-}" ]; then echo "$TOOLS/bin" >> "$GITHUB_PATH"; fi
if [ -n "${GITHUB_ENV:-}" ]; then
  { echo "WASIX_SYSROOT=$SYSROOT"; echo "WASMER=$WASMER"; echo "WASM_DIS=$WASMDIS"; echo "WASM_AS=$(dirname "$WASMDIS")/wasm-as"; } >> "$GITHUB_ENV"
fi
