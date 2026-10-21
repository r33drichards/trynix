# Opt-in Rust feasibility adapter

Use `?experiment=rust&pkg=rustc%401.30.1` on a normally built TryNix site.
This historical compiler is only an architectural baseline, not modern Rust
support. The normal TryNix UI remains unchanged without the experiment flag.

The experiment rejects closures above 256MiB compressed, 768MiB unpacked,
300MiB per unpacked archive, or 64 paths, before archive loading. Signature
verification is mandatory here (unlike the ordinary upstream demo). NAR download,
decode, hash verification and parse run in a dedicated worker, one archive at a
time. Each worker transfers NAR-backed file buffers instead of cloning them.
Worker deadlines and cancellation terminate the worker. Streamed downloads
also enforce an actual byte cap; metadata budgets alone are not security limits.

The VM handle exposes idempotent dispose() using Emscripten's existing
PThread.terminateAllThreads(). Cancel also navigates to a fresh page without
`boot=1`, covering partially initialized engines and releasing page-owned memory.
Live closure extension is disabled in this mode. WebMCP commands are capped at
120 seconds and dispose the guest after each command, successful or not; combine
compile and program execution in one command. This timer requires a responsive
owner event loop; it is not a host OS CPU quota. Manual serial terminal commands
do not pass through that wrapper. Use the supervised tool for the POC fixtures.

Still unverified: modified browser boot/compile, decoder worker browser loading,
peak memory, cancellation during compile, clean cold/warm timing. Unit tests do
not establish those results. QEMU already executes through pthread workers;
its default Wasm reservation is 2300 MiB, separate from 512 MiB guest RAM and the
unpacked store. Decompression and filesystem overhead can exceed budgets.

No engine binary is changed or redistributed by this patch; existing pinned
QEMU build/source/license requirements remain. No guest network adapter or relay
is added. Browser package fetches still use the configured signed Nix caches.

Run `node --test tests/site/*.test.mjs`. Current result: 63 passed, 2 skipped
(bzip2 browser-vendor tests require assembled site/vendor). Real modified-browser
verification must precede any claim that Rust compilation works in pastebin.

## Preview evidence, 2026-10-21 UTC

Temporary preview: https://rust-poc-static-preview.up.railway.app/?experiment=rust
This is a static Railway server, not a compilation service or guest network relay.
No main branch merge or existing service change was performed.

- Deployed revision `25ee78a54f3fb2df99f5dbf9f3c83141ebd47850` built successfully through the existing pinned Nix site derivation. HTTP 200 and COOP/COEP headers were verified.
- Modern Rust 1.99.0: the 13-path closure is rejected before engine and archive downloads because an unpacked archive exceeds the budget.
- Small current hello closure: command returned status 0 and “Hello, world!”; a subsequent console read reported no guest after automatic teardown.
- Revised small XZ hello 2.10: four signed paths, 6.7 MiB compressed. Debug log timestamps were closure 8.9s, engine instantiated 9.5s, VM running 15.4s, guest prompt 32.9s, successful command result 52.6s after page load. These include operator delays and are not isolated benchmark timings.
- Cancel during preparation returned to a fresh, responsive unbooted page.
- Historical Rust 1.30.1: 22 signed paths, about 148 MiB compressed / 517 MiB unpacked. First worker-unpack attempt became unresponsive during closure loading. A single retry after bounded XZ assembly and cooperative materialization again produced a browser DOM timeout before any compiler command. Both tabs were closed successfully.
- No Rust source compile/run, syntax-error, stdin/file, infinite-loop, or cold/warm compilation metric has passed. Node mock-driver tests are not evidence of these browser outcomes.
- The failures do not diagnose OOM. Main-thread stalls, memory pressure, and browser-environment constraints remain hypotheses. Moving decompression alone is insufficient evidence of a fix.
- Latest local suite: 71 tests, 69 passed, two skipped because the local source checkout lacks Nix-assembled bzip2 vendor artifacts.

Do not repeat unchanged heavy launches or increase budgets to make a test pass.
A next architectural investigation is a killable owner worker for the VM and
filesystem, or a backing store that does not retain the complete unpacked closure
in MEMFS. The present Cancel button cannot promise to interrupt a blocked owner
event loop. Keep this PR draft and do not advertise Rust support.
