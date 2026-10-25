# Experimental QEMU/WASIX port

**Local Linux boot + Lua hello passed under Wasmer 7.5.0.** This is a draft experiment, not an Edge-ready service. It retains QEMU and Linux and produces WebAssembly, not a native VM or per-language interpreter.

## Measured milestone

The 2026-10-23 cold guest run completed the init handshake and printed `LUA_HELLO_PASS` and `LUA_EXIT:0` in 63.519 seconds. Artifact: 12,630,670 bytes; peak host RSS: 860,888 KiB (~841 MiB); guest RAM: 512 MiB; TCI cache: 16 MiB. Wasm initial/max memory: 768 MiB/2 GiB. These are different measurements. The supervisor deliberately killed the process group after success, so cleanup returncode -9 is expected.

Artifact SHA-256: `cf295debf9910fd74169b77bc8f1179613ed545786c1db806b3a7ef41e50ef6a`.

See BUILD.md for preparation and RESULTS.md for passes, failures and untested gates. No Edge deployment, snapshot/restore compatibility, hostile-workload isolation or production readiness is established.

## Port design

- QEMU TCI replaces the browser's Emscripten JavaScript translation-block JIT. Exact typed Wasm-local helper thunks replace host `call_dynamic` dispatch. The tested module has no `call_dynamic` import; libffi remains for argument-layout metadata.
- WASIX execution contexts implement thread-affine coroutines; cross-thread entry aborts and coroutine pooling is disabled. 9p I/O is synchronous on the owning thread, not migrated into native workers. The supervisor is the cancellation boundary.
- WASIX host errno values are mapped to Linux values, directory cookies are copied/replayed, and unsupported statvfs/xattr operations return errors rather than fabricated data.
- Pinned WASIX stat drops POSIX mode bits. Build-time `prepare-mapped-share.py` converts the verified package directory into QEMU's mapped-file representation, preserving file type, executable bits and symlink targets independently of host stat. The guest runner selects `security_model=mapped-file`.
- Pinned libc `sigwait` returns success with signal zero in the small probe. The WASIX target therefore omits the POSIX signal-to-fd bridge. TCI kicks use cpu_exit plus condition/event notifications. Native host signal delivery is not part of this contract; guest Linux signals remain emulated. Other accelerators are unsupported.

## Bounded exception workaround

Wasmer 7.5.0 retains exception objects. `cache-longjmp.py` rewrites the pinned compiler's single C-longjmp throw into a fixed 64-entry key/reference cache per Wasmer store. It fails closed on an unexpected tag/throw shape and exits 78 if 64 distinct payload addresses are exceeded. This is a bound over store lifetime, not merely live checkpoints. The cache never evicts or frees runtime objects.

This preserves the pointer payload used by libc's live jmp_buf, but reuses exception identity and its first-allocation backtrace. It is specific to C longjmp, not general exception semantics. The supported embedding serializes each store; new pthread stores get fresh globals. **Wasmer store fork, snapshot, restore, transfer, concurrent shared-store embedding, and host-trap recovery/re-entry are unsupported.** QEMU guest migration is a separate, untested and deferred feature.

Typed-helper code currently uses the legacy internal flag `CONFIG_WASIX_ASYNCIFY`; it does **not** select Asyncify. Normal EH remains enabled. Plugins must remain disabled: their substituted callback pointer is incompatible with fixed-helper thunks. Browser patches and browser snapshots are not interchangeable with this target.

## Capability and safety limits

Required: shared memory/threads, pipes/poll, clocks, entropy, explicitly mounted files and WASIX context switching. Host subprocess creation is ENOSYS; guest Linux fork is separate. GIO, host debugger spawning, shared host-process RAM mappings and no-reserve RAM are unsupported. Guest RAM is aligned zeroed storage within Wasm memory; native PROT_NONE guard pages are unavailable. The whole Wasm instance, not individual allocations, is the isolation boundary.

Use disposable dedicated PACK/SHARE directories only. The guest 9p export is read-only by default and rejected writes in the lifecycle test. The Wasmer host volume mapping itself is still writable; this is not a host-enforced immutable mount. A dedicated writable overlay is not implemented. No host filesystem root or new network grant is requested. `--offline` disables registry resolution, **not networking**; cached runtime permissions can still apply. Enforce network denial externally where required. The runner enforces time/output/RSS limits and kills its process group, but is not a substitute for OS resource isolation.

The original 2300 MiB shared-memory pthread configuration failed locally; 768 MiB initial memory works in this experiment. No hosted Edge quota or context-support claim follows from local success. The final-artifact lifecycle sanity matrix passed: warm Lua reuse, read-only guest export, cancellation of busy Lua with process-group cleanup, and a fresh guest without the prior /tmp canary. This is not evidence against malicious guest escapes or cross-tenant attacks.

Packages are verified/unpacked at build time, not downloaded during execution. This share is not a disk image or RAM snapshot. Snapshot restoration remains deferred.
