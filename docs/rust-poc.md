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
