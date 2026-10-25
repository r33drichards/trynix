#!/usr/bin/env bash
# Install the WASIX toolchain revisions recorded in ports/wasix/pins.json
# (observed_toolchain_dependencies) without any account or registry login:
#   - wasix-org/llvm-project 21.1.206
#   - wasmerio/wasmer v7.5.0 (the LLVM release does not ship wasmer; this was learned the hard way)
#   - wasix-org/wasix-libc v2026-10-02.1 sysroot-exnref-eh
#   - WebAssembly/binaryen version_133
# Publishes wasixcc/wasix++/wasixar/wasixranlib on PATH and records provenance.
# Every failure path prints what it saw: silent exits made the first runs undiagnosable.
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
TOOLS=${TOOLS:-${RUNNER_TEMP:-/tmp/wasix-tools}/tools}
mkdir -p "$TOOLS/bin" "$TOOLS/dl"
: > "$TOOLS/provenance.txt"
fetch(){ local url=$1 name=$2 stats
  if [ -s "$TOOLS/dl/$name" ]; then echo "cached $name bytes=$(wc -c < "$TOOLS/dl/$name")"; return 0; fi
  if ! stats=$(curl -fsSL --retry 3 -o "$TOOLS/dl/$name" -w '%{http_code} %{size_download} %{url_effective}' "$url"); then
    echo "DOWNLOAD FAILED name=$name url=$url code=${stats:-curl-nonzero} bytes=$(wc -c < "$TOOLS/dl/$name" 2>/dev/null || echo 0)"
    exit 1
  fi
  echo "fetched $name stats=$stats on_disk=$(wc -c < "$TOOLS/dl/$name")"
  echo "$name sha256=$(sha256sum "$TOOLS/dl/$name" | cut -d' ' -f1)" >> "$TOOLS/provenance.txt"
}
fetch https://github.com/wasix-org/llvm-project/releases/download/21.1.206/LLVM-Linux-x86_64.tar.gz llvm.tar.gz
fetch https://github.com/wasix-org/wasix-libc/releases/download/v2026-10-02.1/sysroot-exnref-eh.tar.gz sysroot.tar.gz
fetch https://github.com/WebAssembly/binaryen/releases/download/version_133/binaryen-version_133-x86_64-linux.tar.gz binaryen.tar.gz
fetch https://github.com/wasmerio/wasmer/releases/download/v7.5.0/wasmer-linux-amd64.tar.gz wasmer.tar.gz
tar xzf "$TOOLS/dl/llvm.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/sysroot.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/binaryen.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/wasmer.tar.gz" -C "$TOOLS"
locate(){ # locate <label> <find-args...> ; sets global FOUND, prints to stdout
  local label=$1; shift
  mapfile -t HITS < <(find "$TOOLS" "$@" 2>/dev/null | head -1)
  if [ "${#HITS[@]}" -eq 0 ]; then
    echo "NOT FOUND: $label"; echo "--- tree under $TOOLS (maxdepth 3) ---"; find "$TOOLS" -maxdepth 3 | head -40; exit 1
  fi
  FOUND="${HITS[0]}"; echo "found $label=$FOUND"
}
df -h "$TOOLS" | tail -1 | sed "s#^#disk: #"
tar tzf "$TOOLS/dl/llvm.tar.gz" > "$TOOLS/llvm-list.txt" 2> "$TOOLS/llvm-err.txt" || echo "llvm.tar.gz listing failed: $(head -c 160 "$TOOLS/llvm-err.txt")"
head -5 "$TOOLS/llvm-list.txt" | sed "s#^#llvm-entry: #"
echo "== locating toolchain pieces under $TOOLS =="
locate clang \( -type f -o -type l \) -name clang;            CLANG="$FOUND"
locate wasmer \( -type f -o -type l \) -name wasmer;          WASMER="$FOUND"
locate wasm-dis \( -type f -o -type l \) -name wasm-dis;      WASMDIS="$FOUND"
locate crt1-command.o \( -type f -o -type l \) -name 'crt1-command.o'; CRT1="$FOUND"
locate compiler-rt \( -type f -o -type l \) -name 'libclang_rt.builtins-wasm32.a'; BUILTINS="$FOUND"
LLVM=$(dirname "$CLANG"); BINEN=$(dirname "$WASMDIS")
SYSROOT=$(dirname "$(dirname "$CRT1")")
RESROOT=$(dirname "$(dirname "$LLVM")")/lib/clang
echo "== clang resource dir: $RESROOT =="
mapfile -t RESVER < <(find "$RESROOT" -mindepth 1 -maxdepth 1 -type d)
if [ "${#RESVER[@]}" -eq 0 ]; then echo "no clang version dir under $RESROOT"; ls -l "$RESROOT" 2>&1 | head -10; exit 1; fi
RES="${RESVER[0]}/lib"
echo "resource version dir=${RESVER[0]} -> builtins under $RES"
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
  { echo "WASIX_SYSROOT=$SYSROOT"; echo "WASMER=$WASMER"; echo "WASM_DIS=$WASMDIS"; echo "WASM_AS=$BINEN/wasm-as"; } >> "$GITHUB_ENV"
fi
