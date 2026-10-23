# Experimental QEMU/WASIX port

This is an in-progress local port, not an Edge-ready package. It retains QEMU and the Linux guest, producing WebAssembly rather than substituting a native VM or language-specific WASM interpreters.

## Current evidence

- Wasmer 7.5.0 and wasixcc 0.4.7: C hello, pthread join, pipes, and WASIX execution contexts pass locally.
- 768 MiB initial memory / 2 GiB maximum: boundary probes pass. This is not a measurement of QEMU guest memory or peak RSS.
- Fresh-instance filesystem/global-state probes pass twice; external process-group cancellation and clean restart pass. This does not establish security isolation under hostile QEMU workloads.
- Official WASIX libffi mixed-width ffi_call passes. WASIX static zlib, GLib, GObject, GThread and GModule compile. GIO is excluded.
- QEMU cross-configuration succeeds with the TCG interpreter and candidate WASIX coroutine backend. Full compilation and linking succeeded; QEMU 8.2.0 prints its version under Wasmer. A clean build-script reproduction passed (1,512 build steps). The debugless module is 12,689,356 bytes (after the synchronous 9p adjustment).
- WASIX host-to-Linux errno mapping and directory cookie seek/replay pass. Host statvfs returns ENOTSUP locally, which the guest must handle; no disk capacity is fabricated.
- sigsetjmp fails in a standalone WASIX EH probe; setjmp and a longjmp across official libffi both pass. The three CPU/translator jump sites that request no signal-mask saving use plain setjmp.
- Linux boot, Lua inside Linux, migration restore, full guest cancellation/isolation, and Edge hosting have NOT passed yet.

The existing browser artifact is not used unchanged: its Emscripten JS translation-block compiler is replaced by QEMU TCI. This trades JIT performance for a runtime-independent interpreter. The candidate coroutine backend uses WASIX contexts and explicitly aborts if a coroutine crosses host threads, even in release builds. Coroutine pooling is disabled. 9p operations run synchronously on the owning thread rather than migrating WASIX contexts into the native worker pool. This can block QEMU I/O; the external supervisor is the cancellation boundary. Actual Linux 9p attach/read remains untested.

## Known runtime limits and capability contract

The original 2300 MiB initial shared-memory configuration reproduces a Wasmer WASIX thread-spawn overflow locally, although a plain Wasm module with that memory size starts. A 768 MiB starting allocation succeeds in probes; full QEMU must still validate its own budget. The guest remains configured for 512 MiB. Artifact size, linear-memory reservation and host RSS are distinct measurements.

Host subprocess creation is not provided by the chosen WASIX exception-handling ABI. host-compat.h returns ENOSYS for GLib host fork paths; it never reports successful subprocess creation. Guest Linux fork is a different operation and is not stubbed by this shim. GIO, host debugger spawning, coroutine migration across host threads and browser snapshot interchange are unsupported pending tests.

The port needs threads, pipes/poll, clocks, entropy, files and WASIX context switching. Guest storage must be explicitly mounted per instance; do not grant host filesystem roots or networking by default. --offline only disables registry resolution, not networking; inherited runtime network permissions can still apply. Strict denial requires externally enforced network isolation. A supervisor must enforce deadlines and kill the entire instance process group. No Edge memory quota, host context support or deployment compatibility is asserted by these local probes.

## Boundary probe

With official wasixcc and wasmer on PATH, run `python3 ports/wasix/test-capabilities.py`. The script emits JSON and asserts the expected results. This test does not boot QEMU. See BUILD.md for the build sequence and RESULTS.md for measured failures as well as passes.

## Host-memory limitations

The TCI bytecode buffer is RW data, not executable code. Anonymous guest RAM uses aligned, zeroed allocation inside Wasm linear memory. Shared host-process mappings and no-reserve RAM are rejected. WASIX cannot provide native PROT_NONE guard pages; the whole Wasm instance remains the isolation boundary, not individual allocations within it. The mprotect adapter returns success only for already-RW data, and ENOTSUP for other protection requests.

This experiment starts from the pinned qemu-wasm fork, not a native QEMU replacement. Browser-only Emscripten JIT/FS patches are not applied to the TCI/WASIX target. It uses the pinned TryNix kernel, initramfs and machine definition, reducing the TCG bytecode cache to 16 MiB. Existing browser migration snapshots are not assumed compatible.

## Blocking result

The module reaches SeaBIOS and Linux early decompression, but **does not yet boot Linux or run Lua**. Repeated setjmp/longjmp causes growing host RSS on tested Wasmer Cranelift and LLVM backends while Wasm linear-memory pages remain constant. A one-million-jump reproducer peaks near 494 MiB (Cranelift) or 525 MiB (LLVM). V8 stays near 82 MiB but rejects WASIX context creation and fails the required libffi/longjmp probe. It is not a working alternative for this port.

Wasmer 7.5.0 retains exception objects, payloads and backtraces in its store, with reference counting still marked TODO. This is consistent with an upstream-described EH memory leak, but we have not instrumented every byte of the QEMU run. No unsafe exception-object recycling or custom runtime patch is included. A production-quality path needs bounded runtime exception handling or a verified alternative control-flow design.

The package closure can already be verified and unpacked at build time with prepare-lua.mjs (Node 24, curl, xz and bzip2 required). This produces a new host directory, not a disk image or RAM snapshot. The bounded local runner uses that prepopulated share without downloading packages at execution time. Read-only package mounts and a writable overlay remain unimplemented. A booted snapshot might bypass boot work but does not fix execution-time exception growth; migration compatibility remains untested and deferred.
