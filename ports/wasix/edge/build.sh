#!/usr/bin/env bash
set -euo pipefail
PORT=$(cd "$(dirname "$0")" && pwd)
OUT=${1:?output directory required}
mkdir -p "$OUT"
wasixcc -O2 -pthread -Wall -Wextra -Werror "$PORT/exec-api.c" -o "$OUT/exec-api.wasm"
wasixcc -O2 -Wall -Wextra -Werror "$PORT/fake-guest.c" -o "$OUT/fake-guest.wasm"
