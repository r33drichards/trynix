#!/usr/bin/env bash
# pins.json records both the upstream libffi release (libffi.tar.gz)
# and the wasix-org fork commit archive (libffi-pinned.tar.gz); the build
# uses the fork, so the fork hash is the one verified here.
# Fetch pinned sources, verify checksums against ports/wasix/pins.json, apply the
# WASIX patches (including the coroutine C-stack gate files) and run the port's
# dependency + QEMU build scripts. Requires ci-toolchain.sh output on PATH.
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
W=${WORKDIR:-${RUNNER_TEMP:-/tmp/wasix-build}}
mkdir -p "$W/src" "$W/x" "$W/out"
JOBS=${JOBS:-3}
URLS='qemu-wasm|https://github.com/ktock/qemu-wasm/archive/0ef7b4e2814b231705d8371dd7997f5b72e70baf.tar.gz|qemu.tar.gz
qemu-upstream|https://download.qemu.org/qemu-8.2.0.tar.xz|qemu-upstream.tar.xz
libffi|https://github.com/wasix-org/libffi/archive/09cbf7d66d232a01dbb0c88fd5ae65fa9c15f7c7.tar.gz|libffi-pinned.tar.gz
glib|https://download.gnome.org/sources/glib/2.75/glib-2.75.0.tar.xz|glib.tar.gz
zlib|https://zlib.net/fossils/zlib-1.3.1.tar.xz|zlib.tar.xz'
echo "== download + verify =="
while IFS='|' read -r name url pin; do
  [ -n "${name:-}" ] || continue
  f="$W/src/$name-$(basename "$url")"
  curl -fsSL --retry 3 -o "$f" "$url"
  got=$(sha256sum "$f" | cut -d' ' -f1)
  want=$(python3 -c "import json,sys;print(json.load(open('$PORT/pins.json'))['archives_sha256'].get('$pin',''))")
  if [ -n "$want" ] && [ "$want" != "$got" ]; then echo "CHECKSUM MISMATCH $name: $got != $want"; exit 1; fi
  if [ -z "$want" ]; then echo "UNVERIFIED $name sha256=$got (no matching entry in pins.json)"; else echo "verified $name sha256=$got"; fi
done <<EOF
$URLS
EOF
echo "== extract =="
tar xzf "$W/src/qemu-wasm-0ef7b4e2814b231705d8371dd7997f5b72e70baf.tar.gz" -C "$W/x"
tar xf  "$W/src/qemu-upstream-qemu-8.2.0.tar.xz" -C "$W/x"
tar xzf "$W/src/libffi-09cbf7d66d232a01dbb0c88fd5ae65fa9c15f7c7.tar.gz" -C "$W/x"
tar xf  "$W/src/glib-glib-2.75.0.tar.xz" -C "$W/x"
tar xf  "$W/src/zlib-zlib-1.3.1.tar.xz" -C "$W/x"
QEMU=$(find "$W/x" -maxdepth 1 -type d -name 'qemu-wasm-*' | head -1)
UP=$(find "$W/x" -maxdepth 1 -type d -name 'qemu-8.2.0' | head -1)
FFI=$(find "$W/x" -maxdepth 1 -type d -name 'libffi-*' | head -1)
GLIB=$(find "$W/x" -maxdepth 1 -type d -name 'glib-*' | head -1)
ZLIB=$(find "$W/x" -maxdepth 1 -type d -name 'zlib-*' | head -1)
for sm in dtc keycodemapdb berkeley-softfloat-3 berkeley-testfloat-3; do
  rm -rf "$QEMU/subprojects/$sm"; cp -a "$UP/subprojects/$sm" "$QEMU/subprojects/$sm"
done
echo "== patches =="
(cd "$FFI"  && patch -p1 < "$PORT/libffi-wasix.patch")
(cd "$GLIB" && patch -p1 < "$PORT/glib-minimal.patch")
(cd "$QEMU" && patch -p1 < "$PORT/qemu-wasix.patch")
(cd "$QEMU" && patch -p1 < "$PORT/qemu-wasix-runtime.patch")
(cd "$QEMU" && patch -p1 < "$PORT/qemu-wasix-coroutine-stack.patch")
cp "$PORT/coroutine-wasix.c" "$QEMU/util/coroutine-wasix.c"
cp "$PORT/coroutine-wasix-switch.S" "$QEMU/util/coroutine-wasix-switch.S"
cp "$PORT/9p-errno-wasix.inc" "$QEMU/hw/9pfs/9p-errno-wasix.inc"
echo "== dependency build =="
bash "$PORT/build-deps.sh" "$FFI" "$ZLIB" "$GLIB" "$W/out/deps" "$W/out/deps-build"
echo "== QEMU build =="
bash "$PORT/build-qemu.sh" "$QEMU" "$W/out/deps" "$W/out/qemu"
ls -la "$W/out/qemu/qemu-system-x86_64.wasm"
sha256sum "$W/out/qemu/qemu-system-x86_64.wasm" | tee "$W/out/qemu/qemu-system-x86_64.wasm.sha256"
echo "== coroutine C-stack gate (fails on the unfixed backend) =="
bash "$PORT/test-coroutine-stack.sh" 1500
