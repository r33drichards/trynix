#!/usr/bin/env bash
# Inputs: clean extracted sources with libffi-wasix/glib-minimal patches applied.
# Requires official wasixcc, Meson, Ninja, Autoconf/Automake/Libtool and pkg-config.
set -euo pipefail
if [[ $# != 5 ]]; then echo "usage: $0 LIBFFI_SOURCE ZLIB_SOURCE GLIB_SOURCE PREFIX BUILD_DIR" >&2; exit 2; fi
FFI=$(realpath "$1"); ZLIB=$(realpath "$2"); GLIB=$(realpath "$3")
mkdir -p "$4" "$5"; DEPS=$(realpath "$4"); BUILD=$(realpath "$5")
PORT=$(cd "$(dirname "$0")" && pwd)
export CC=wasixcc AR=wasixar RANLIB=wasixranlib CFLAGS='-O2 -pthread'
(cd "$FFI" && ./autogen.sh && ./configure --host=wasm32-unknown-wasi --prefix="$DEPS" --enable-static --disable-shared --disable-docs && make -j"${JOBS:-4}" && make install)
(cd "$ZLIB" && ./configure --prefix="$DEPS" --static && make -j"${JOBS:-4}" && make install)
python3 - "$PORT" "$DEPS" "$BUILD/wasix.cross" <<'PY'
import sys
from pathlib import Path
port,deps,out=sys.argv[1:]
# repr produces valid Meson string literals for ordinary POSIX paths.
s=f'''[binaries]
c = ['python3', {port+'/cc-wrapper.py'!r}, 'wasixcc']
cpp = ['python3', {port+'/cc-wrapper.py'!r}, 'wasix++']
ar = 'wasixar'
ranlib = 'wasixranlib'
pkg-config = 'pkg-config'
exe_wrapper = ['wasmer', 'run']
[host_machine]
system = 'wasi'
cpu_family = 'wasm32'
cpu = 'wasm32'
endian = 'little'
[properties]
needs_exe_wrapper = true
pkg_config_libdir = {deps+'/lib/pkgconfig'!r}
[built-in options]
c_args = ['-O2', '-pthread', '-include', {port+'/host-compat.h'!r}]
cpp_args = ['-O2', '-pthread']
c_link_args = ['-pthread']
cpp_link_args = ['-pthread']
pkg_config_path = {deps+'/lib/pkgconfig'!r}
'''
Path(out).write_text(s)
PY
meson setup "$BUILD/glib" "$GLIB" --prefix="$DEPS" --libdir=lib \
 --cross-file="$BUILD/wasix.cross" --default-library=static --buildtype=release \
 -Db_staticpic=false -Dselinux=disabled -Dxattr=false -Dlibmount=disabled \
 -Dnls=disabled -Dtests=false -Dglib_assert=false -Dglib_checks=false
ninja -C "$BUILD/glib" -j"${JOBS:-4}"
meson install -C "$BUILD/glib"
