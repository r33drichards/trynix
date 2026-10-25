#!/usr/bin/env bash
# Install the toolchain the port was actually built with: official wasixcc 0.4.7
# (its installer fetches the pinned WASIX LLVM/sysroot/binaryen into ~/.wasixcc)
# plus an explicit Wasmer 7.5.0 binary, because pins.json records wasmer 7.5.0 and
# the installer's own wasmer may differ.
#
# Earlier revisions of this script tried to emulate wasixcc with a raw clang
# --target=wasm32-wasixp1 wrapper plus a hand-written crt1.o linker shim. That
# emulation kept failing in ways the real driver does not (configure's
# "C compiler cannot create executables", then "undefined symbol: main"), so it
# was removed rather than patched further: use the upstream tool.
set -euo pipefail
TOOLS=${TOOLS:-${RUNNER_TEMP:-/tmp/wasix-tools}/tools}
mkdir -p "$TOOLS/bin" "$TOOLS/dl"
: > "$TOOLS/provenance.txt"
say(){ echo "[toolchain] $*"; }
say "installing wasixcc via https://wasix.cc (non-interactive)"
if ! curl -fsSL --retry 3 https://wasix.cc | sh; then say "installer failed"; exit 1; fi
export PATH="$HOME/.wasixcc/bin:$TOOLS/bin:$PATH"
say "installed under \$HOME/.wasixcc:"; ls -1 "$HOME/.wasixcc" 2>&1 | head -10; ls -1 "$HOME/.wasixcc/bin" 2>&1 | head -15
say "wasixcc --version:"; wasixcc --version 2>&1 | head -5 || say "wasixcc --version exited \$?"
printf 'int main(void){return 0;}\n' > "$TOOLS/smoke.c"
say "smoke compile+link:"; wasixcc -O2 -pthread "$TOOLS/smoke.c" -o "$TOOLS/smoke.wasm" 2>&1 | head -15 || say "smoke link failed \$?"
say "wasixccenv:"; (wasixccenv --version 2>&1 | head -2) || true
# wasixcc behaves differently with and without -pthread on this runner; pin the
# difference down explicitly so the dependency build is not debugged blind.
printf "int main(void){return 0;}" > "$TOOLS/v.c"
say "variant A (no -pthread):"; wasixcc -O2 "$TOOLS/v.c" -o "$TOOLS/vA.wasm" 2>&1 | tail -4; say "A rc=$? exists=$([ -s "$TOOLS/vA.wasm" ] && echo yes || echo no)"
say "variant B (-pthread):"; wasixcc -O2 -pthread "$TOOLS/v.c" -o "$TOOLS/vB.wasm" 2>&1 | tail -4; say "B rc=$? exists=$([ -s "$TOOLS/vB.wasm" ] && echo yes || echo no)"
say "variant C (-c only):"; wasixcc -O2 -c "$TOOLS/v.c" -o "$TOOLS/vC.o" 2>&1 | tail -3; say "C exists=$([ -s "$TOOLS/vC.o" ] && echo yes || echo no)"

say "fetching pinned Wasmer 7.5.0 runtime"
curl -fsSL --retry 3 -o "$TOOLS/dl/wasmer.tar.gz" https://github.com/wasmerio/wasmer/releases/download/v7.5.0/wasmer-linux-amd64.tar.gz
tar xzf "$TOOLS/dl/wasmer.tar.gz" -C "$TOOLS"
WASMER=$(find "$TOOLS" \( -type f -o -type l \) -name wasmer | head -1)
[ -n "$WASMER" ] || { say "wasmer binary not found after extract"; find "$TOOLS" -maxdepth 3 | head -30; exit 1; }
if [ "$WASMER" != "$TOOLS/bin/wasmer" ]; then cp -f "$WASMER" "$TOOLS/bin/wasmer"; fi
chmod +x "$TOOLS/bin/wasmer"
say "wasmer (pinned):"; "$TOOLS/bin/wasmer" --version | head -1
if [ -s "$TOOLS/smoke.wasm" ]; then say "smoke run:"; "$TOOLS/bin/wasmer" run "$TOOLS/smoke.wasm" 2>&1 | head -5; say "smoke rc=$?"; else say "smoke.wasm was not produced"; fi
for f in "$TOOLS/dl/wasmer.tar.gz" "$HOME/.wasixcc/bin/wasixcc"; do
  [ -e "$f" ] && echo "$(basename "$f") sha256=$(sha256sum "$f" | cut -d' ' -f1)" >> "$TOOLS/provenance.txt"
done
cat "$TOOLS/provenance.txt"
if [ -n "${GITHUB_PATH:-}" ]; then { echo "$TOOLS/bin"; echo "$HOME/.wasixcc/bin"; } >> "$GITHUB_PATH"; fi
if [ -n "${GITHUB_ENV:-}" ]; then
  echo "WASMER=$TOOLS/bin/wasmer" >> "$GITHUB_ENV"
  echo "WASIX_HOME=$HOME/.wasixcc" >> "$GITHUB_ENV"
  B=$(find "$HOME/.wasixcc" \( -type f -o -type l \) -name 'wasm-dis' | head -1)
  [ -n "$B" ] && { echo "WASM_DIS=$B" >> "$GITHUB_ENV"; echo "WASM_AS=$(dirname "$B")/wasm-as" >> "$GITHUB_ENV"; }
fi
say "done"
