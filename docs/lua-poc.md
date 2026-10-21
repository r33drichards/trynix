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

Infinite-loop deadline and manual cancellation tests on the revised preview
are pending until recorded below.
