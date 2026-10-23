#!/usr/bin/env bash
# Build already prepared pinned sources. See README for experimental limitations.
set -euo pipefail
if [[ $# != 3 ]]; then echo "usage: $0 QEMU_SOURCE DEPENDENCY_PREFIX BUILD_DIR" >&2; exit 2; fi
SOURCE=$(realpath "$1")
DEPS=$(realpath "$2")
mkdir -p "$3"; BUILD=$(realpath "$3")
PORT=$(cd "$(dirname "$0")" && pwd)
export AR=wasixar RANLIB=wasixranlib NM=wasixnm
export PKG_CONFIG_LIBDIR="$DEPS/lib/pkgconfig"
# Patches must be applied once before invoking; no source edits here.
cd "$BUILD"
"$SOURCE/configure" --cross-prefix= \
 --cc="python3 $PORT/cc-wrapper.py wasixcc" \
 --cxx="python3 $PORT/cc-wrapper.py wasix++" \
 --cpu=wasm32 --target-list=x86_64-softmmu \
 --enable-tcg-interpreter --disable-coroutine-pool --with-coroutine=wasix \
 --disable-werror --disable-docs --disable-tools --disable-user \
 --disable-guest-agent --disable-plugins --disable-slirp --disable-gtk \
 --disable-sdl --disable-vnc --disable-opengl --disable-virglrenderer \
 --disable-pixman --disable-capstone --enable-fdt=internal --disable-download \
 --disable-tpm --disable-colo-proxy --disable-replication \
 --extra-cflags='-O2 -pthread -DCONFIG_WASIX_ASYNCIFY' \
 --extra-ldflags='-pthread -Wl,--initial-memory=805306368 -Wl,--max-memory=2147483648'
ninja -j"${JOBS:-4}" qemu-system-x86_64
# Legacy feature name selects typed helper thunks, NOT Asyncify. Normal EH stays enabled.
python3 "$PORT/strip-debug.py" qemu-system-x86_64 qemu-system-x86_64.uncached.wasm
WASM_DIS=${WASM_DIS:-$HOME/.wasixcc/binaryen/bin/wasm-dis}
WASM_AS=${WASM_AS:-$HOME/.wasixcc/binaryen/bin/wasm-as}
"$WASM_DIS" qemu-system-x86_64.uncached.wasm -o qemu-system-x86_64.wat
python3 "$PORT/cache-longjmp.py" qemu-system-x86_64.wat qemu-system-x86_64.cached.wat
"$WASM_AS" qemu-system-x86_64.cached.wat -o qemu-system-x86_64.wasm \
 --enable-exception-handling --enable-reference-types --enable-bulk-memory \
 --enable-simd --enable-relaxed-simd --enable-extended-const --enable-threads \
 --enable-mutable-globals --enable-sign-ext --enable-nontrapping-float-to-int --enable-multivalue
