#!/bin/sh
# Native control-flow tests only. All generated artifacts go in OUTPUT_DIRECTORY.
set -eu
if [ "$#" -ne 1 ]; then
    printf 'Usage: %s OUTPUT_DIRECTORY\n' "$0" >&2
    exit 2
fi
test_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
mkdir -p "$1"
output_dir=$(CDPATH= cd "$1" && pwd)
source_file="$test_dir/exec-api.c"
binary="$output_dir/test-quarantine"
log="$output_dir/test-quarantine.log"
compiler=${CC:-cc}
export TMPDIR="$output_dir"
result=0
{
    before=$(sha256sum "$source_file")
    printf 'Source before: %s\n' "$before"
    if "$compiler" -std=c11 -O0 -g -Wall -Wextra -Werror -pthread \
        "$test_dir/test-quarantine.c" -o "$binary"; then
        "$binary" || result=$?
    else
        result=$?
    fi
    after=$(sha256sum "$source_file")
    printf 'Source after:  %s\n' "$after"
    if [ "$before" != "$after" ]; then
        printf '%s\n' 'Source changed during this test; rerun required.'
        result=1
    fi
    printf 'Exit status: %s\n' "$result"
} > "$log" 2>&1
cat "$log"
exit "$result"
