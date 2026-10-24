#!/usr/bin/env bash
# Install the WASIX toolchain revisions recorded in ports/wasix/pins.json
# (observed_toolchain_dependencies) without an account or registry login:
#   - wasix-org/llvm-project 21.1.206 (bundles Wasmer 7.5.0)
#   - wasix-org/wasix-libc v2026-10-02.1 sysroot-exnref-eh
#   - WebAssembly/binaryen version_133
# Exposes wasixcc/wasix++/wasixar/wasixranlib/wasmer on $GITHUB_PATH.
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
TOOLS=${TOOLS:-${RUNNER_TEMP:-/tmp/wasix-tools}/tools}
mkdir -p "$TOOLS/bin" "$TOOLS/dl"
SHA(){ sha256sum "$1" | cut -d' ' -f1; }
fetch(){ local url=$1 name=$2
  if [ ! -s "$TOOLS/dl/$name" ]; then curl -fsSL --retry 3 -o "$TOOLS/dl/$name" "$url"; fi
  echo "$(basename "$name") sha256=$(SHA "$TOOLS/dl/$name")" >> "$TOOLS/provenance.txt"
}
: > "$TOOLS/provenance.txt"
fetch https://github.com/wasix-org/llvm-project/releases/download/21.1.206/LLVM-Linux-x86_64.tar.gz llvm.tar.gz
fetch https://github.com/wasix-org/wasix-libc/releases/download/v2026-10-02.1/sysroot-exnref-eh.tar.gz sysroot.tar.gz
fetch https://github.com/WebAssembly/binaryen/releases/download/version_133/binaryen-version_133-x86_64-linux.tar.gz binaryen.tar.gz
tar xzf "$TOOLS/dl/llvm.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/sysroot.tar.gz" -C "$TOOLS"
tar xzf "$TOOLS/dl/binaryen.tar.gz" -C "$TOOLS"
set +o pipefail
LLVM=$(find "$TOOLS" -maxdepth 3 -type d -name bin | grep -F llvm | head -1 || true)
SYSROOT=$(find "$TOOLS" -maxdepth 5 -type d -path '*sysroot*/sysroot' | head -1 || true)
BINEN=$(find "$TOOLS" -maxdepth 3 -type d -name bin | grep -F binaryen | head -1 || true)
RES=$(find "$TOOLS" -maxdepth 4 -type d -path '*lib/clang/*' | head -1 || true)
BUILTINS=$(find "$SYSROOT" -name 'libclang_rt.builtins-wasm32.a' | head -1 || true)
WASM_BIN=$(find "$TOOLS" -maxdepth 4 -type f -name wasmer | head -1 || true)
set -o pipefail
test -n "$LLVM"; test -n "$SYSROOT"
# The release tarball ships no wasm compiler-rt in clang's resource dir, but the
# sysroot provides libclang_rt.builtins-wasm32.a. Link it where clang looks so
# plain "wasixcc file.c -o out.wasm" works like the official wrapper.
if [ -n "${RES:-}" ] && [ -n "${BUILTINS:-}" ]; then
  for t in wasm32-unknown-wasixp1 wasm32-unknown-wasi-preview1-threads wasm32-unknown-wasi; do
    mkdir -p "$RES/lib/$t"; ln -sf "$BUILTINS" "$RES/lib/$t/libclang_rt.builtins.a"
  done
  echo "linked compiler-rt into $RES/lib"
fi
cat > "$TOOLS/bin/wasixcc" <<EOF
#!/bin/sh
exec "$LLVM/clang" --target=wasm32-wasixp1 --sysroot="$SYSROOT" "$@"
EOF
cat > "$TOOLS/bin/wasix++" <<EOF
#!/bin/sh
exec "$LLVM/clang++" --target=wasm32-wasixp1 --sysroot="$SYSROOT" "$@"
EOF
printf '#!/bin/sh\nexec %s/llvm-ar "$@"\n' "$LLVM" > "$TOOLS/bin/wasixar"
printf '#!/bin/sh\nexec %s/llvm-ranlib "$@"\n' "$LLVM" > "$TOOLS/bin/wasixranlib"
printf '#!/bin/sh\nexec %s/wasm-ld "$@"\n' "$LLVM" > "$TOOLS/bin/wasm-ld-real"
chmod +x "$TOOLS"/bin/*
export PATH="$TOOLS/bin:$LLVM:$BINEN:$PATH"
if [ -n "${GITHUB_ENV:-}" ]; then
  echo "$TOOLS/bin:$LLVM:$BINEN" >> "$GITHUB_PATH"
  echo "WASIX_SYSROOT=$SYSROOT" >> "$GITHUB_ENV"
  echo "WASM_DIS=$BINEN/wasm-dis" >> "$GITHUB_ENV"
  echo "WASM_AS=$BINEN/wasm-as" >> "$GITHUB_ENV"
fi
"$TOOLS/bin/wasixcc" --version | head -2
find "$TOOLS" -maxdepth 4 -type f -name wasmer | head -1 | sed "s#^#export WASMER=#" >> "${GITHUB_ENV:-/dev/null}"
cat "$TOOLS/provenance.txt"
