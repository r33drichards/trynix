# Lua Linux-in-browser POC

Lighter follow-up to the negative Rust gate in [rust-poc.md](rust-poc.md).
Not yet integrated into pastebin.

## Verified 2026-10-21 UTC

Catalog Lua 5.5.1:
`/nix/store/1xi0ly6fi0mw561qmv2mf3r20v17frl9-lua-5.5.1`.
Cache preflight: six paths, 12,514,847 compressed bytes (11.94 MiB),
43,686,496 unpacked bytes (41.66 MiB). Browser verified all six signatures.

Real browser QEMU source execution on revision
`1563f4f399ed6ca1dade0694cdc16d58a09475f6` passed:

- Source file exit 0; stdout `hello Lua\nstdin:file\n`.
- Separately captured stderr `diagnostic\n`.
- Lua read both stdin and an input file.
- Invalid source exit 1 with parser diagnostic.
- Explicit `os.exit(7)` returned 7.
- Automatic VM teardown after the supervised command.
- Fresh VM lacked the prior marker file and computed 6\*7=42.

Reproduction: `tools/lua-probe.mjs` generates one command with base64 source,
stdin and input files, bounded output files, and distinct result markers.
These fixture bounds are not a complete security sandbox.

First run logs: closure at 55.9s after page load, engine 56.7s, VM started
65.2s, prompt 83.9s. Second fresh VM: closure 10.6s, engine 10.9s, VM started
19.3s, prompt 37.1s. Operator delays precede Boot. First run had mixed cached
assets; second reused cached assets. These are not controlled cold/warm
benchmarks or isolated execution latencies.

WebMCP timed out before the first result completed. The retained page log
subsequently recorded full successful output; the invocation was not repeated.

## Guarded Lua mode

`?experiment=lua&pkg=lua%405.5.1` uses the existing signature, size,
background-worker, one-command and teardown controls, without larger budgets.
Rust remains unverified. The static server neither executes programs nor
proxies guest networking.

Queued PTY output caused a Ghostty error after terminal disposal. Revised
cleanup terminates QEMU pthreads immediately but retains the inert terminal
until page reset, avoiding writes to a destroyed render target.

## Revised preview checks

Railway deployed code revision `daf86d3093d33fe86095e3c7bd128ab0e441773d`
successfully. Public HTTP 200, COOP same-origin and COEP require-corp verified.

A supervised infinite-loop command with a 10,000 ms deadline produced
`Linux POC command failed: Guest deadline exceeded`; the page remained
responsive and read-console confirmed no guest was running afterward.
The WebMCP call itself timed out, so the retained page log was inspected,
not the command repeated. Loop-entry output was not retained: this proves
command deadline cleanup, not a measured ten seconds of Lua CPU execution.
Clicking Cancel then reset the page to its unbooted Lua selection.

Manual cancellation passed on a separate fresh VM: the terminal ran
`lua -e 'io.write("LOOP_STARTED\n"); io.flush(); while true do end'`.
Read-console showed the separate `LOOP_STARTED` output and no returned prompt.
Clicking Cancel Linux experiment immediately reset to the unbooted page.

Local suite: 72 tests, 70 passed, two existing Nix-vendored bzip2 tests skipped.
No controlled cold/warm benchmark or browser memory measurement was made.
The browser still reserves the same QEMU memory as the Rust experiment;
Lua only makes the fetched toolchain much smaller.

Preview: https://rust-poc-static-preview.up.railway.app/?experiment=lua&pkg=lua%405.5.1
It remains an isolated, billable static preview, not a production deployment.
