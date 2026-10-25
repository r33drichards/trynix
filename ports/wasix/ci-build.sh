#!/usr/bin/env bash
# pins.json records both the upstream libffi release (libffi.tar.gz)
# and the wasix-org fork commit archive (libffi-pinned.tar.gz); the build
# uses the fork, so the fork hash is the one verified here.
# Fetch pinned sources
# zlib: zlib.net/fossils serves only .tar.gz for 1.3.1, whose hash is NOT the pinned
# one; the pinned zlib.tar.xz hash 38ef96b8... matches distfiles.macports.org, so
# that mirror is used and the hash is still verified against pins.json.
# glib: pins.json keys the archive as glib.tar.xz., verify checksums against ports/wasix/pins.json, apply the
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
glib|https://download.gnome.org/sources/glib/2.75/glib-2.75.0.tar.xz|glib.tar.xz
zlib|https://distfiles.macports.org/zlib/zlib-1.3.1.tar.xz|zlib.tar.xz'
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
# libffi/zlib configure scripts execute their compiler sanity conftest on the
# build host. A wasm conftest cannot run on x86_64, so seed the usual autoconf
# (Removed: seeding ac_cv_/lt_cv_ cache answers made later libtool tests
# patches are untouched by this.
# The port's dependency configure scripts run their conftests (autoconf
# "cannot run C compiled programs"). Upstream builds of this port rely on wasm
# binaries being executable on the build host, so register Wasmer as the wasm
# binfmt handler when the runner allows it; otherwise report clearly instead of
# silently faking cache answers for every run test.
echo "== wasm conftest execution capability =="
ls -d /proc/sys/fs/binfmt_misc 2>&1 | head -2 || true
wasmer binfmt --help 2>&1 | head -12 || true
if [ -w /proc/sys/fs/binfmt_misc/register ] || sudo -n true 2>/dev/null; then
  echo "attempting wasmer binfmt register"
  WBIN=$(command -v wasmer)
  { sudo -n "$WBIN" binfmt register 2>&1 || "$WBIN" binfmt register 2>&1 || echo "binfmt registration failed"; } | head -8
  ls /proc/sys/fs/binfmt_misc 2>&1 | head -6
else
  echo "no binfmt access on this runner"
fi
printf "int main(void){return 0;}" > /tmp/bf.c && wasixcc -O2 /tmp/bf.c -o /tmp/bf.wasm 2>/dev/null && (/tmp/bf.wasm; echo "direct exec rc=$?") || true


echo "== binfmt state at build start =="
ls /proc/sys/fs/binfmt_misc 2>&1 | head -8
for e in /proc/sys/fs/binfmt_misc/*; do case "$e" in *register*|*status*) continue;; esac; echo "-- $e"; head -4 "$e" 2>/dev/null; done
printf 'int main(void){return 7;}' > "$W/execprobe.c"
wasixcc -O2 -pthread "$W/execprobe.c" -o "$W/execprobe.wasm" 2>&1 | head -4
{ "$W/execprobe.wasm"; echo "execprobe rc=$? (expect 7 if wasm ran)"; } || true
{ wasmer run "$W/execprobe.wasm" >/dev/null 2>&1; echo "wasmer-run rc=$? (expect 7)"; } || true
echo "== dependency build =="
if ! { bash "$PORT/build-deps.sh" "$FFI" "$ZLIB" "$GLIB" "$W/out/deps" "$W/out/deps-build" 2>&1 | tee "$W/deps.out" | tail -n 0; }; then
  echo "== build-deps failed; collecting diagnostics =="
  printf 'int main(void){return 0;}\n' > "$W/conftest.c"
  echo "--- direct wasixcc link ---"
  wasixcc -O2 -pthread "$W/conftest.c" -o "$W/conftest.wasm" 2>&1 | head -20 || true
  echo "--- conftest.c bytes ---"; wc -c "$W/conftest.c" || true; cat "$W/conftest.c" || true
  echo "--- wasixcc -### ---"
  wasixcc -O2 -pthread "$W/conftest.c" -o "$W/conftest.wasm" -### 2>&1 | tail -3 | cut -c1-700 || true
  echo "--- generated shim ---"
  cat "$(command -v wasix-ld)" 2>/dev/null | head -14 || true
  for f in $(find "$W/x" -maxdepth 3 -name config.log | head -2); do
    echo "--- $f ---"; grep -iE "configure:argv|error|cannot|unrecognized|No such|wasm-ld" "$f" | head -18 || true
    echo "--- config.log tail (raw) ---"; tail -32 "$f" | cut -c1-190 || true
  done
  { echo "::error::dep-build failed; tail follows"; }
  tail -12 "$W/deps.out" 2>/dev/null | sed -E "s/%/%25/g; s/\r/%0D/g; s/\n/%0A/g" | sed -E "s/^::error::dep-tail /::error::/" || true
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    { echo "### dependency build failure (last 45 log lines)"; echo "```"; tail -45 "$W/deps.out" 2>/dev/null; echo "```"; } >> "$GITHUB_STEP_SUMMARY" || true
  fi
  exit 1
fi
echo "== QEMU build =="
bash "$PORT/build-qemu.sh" "$QEMU" "$W/out/deps" "$W/out/qemu"
ls -la "$W/out/qemu/qemu-system-x86_64.wasm"
sha256sum "$W/out/qemu/qemu-system-x86_64.wasm" | tee "$W/out/qemu/qemu-system-x86_64.wasm.sha256"
echo "== coroutine C-stack gate (fails on the unfixed backend) =="
bash "$PORT/test-coroutine-stack.sh" 1500
