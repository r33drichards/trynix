# Local results, 2026-10-23 UTC

Experimental, blocked before Linux userspace. No hosted deployment was performed.

## Passed

- Wasmer 7.5.0, wasixcc 0.4.7 (LLVM 21.1.2).
- Static libffi mixed u32/u64 arguments and return; 100,000 repeated calls.
- pthread, pipe, WASIX execution-context switch and fresh-instance filesystem/global-state probes.
- Boundary loop starts, is killed by the external supervisor deadline, then a fresh instance passes again. This is NOT guest Linux cancellation/isolation validation.
- 9p host errno conversion and directory-cookie seek/replay probes. statvfs correctly fails ENOTSUP on this host.
- Fresh dependency build script replay on pristine pinned libffi, zlib and GLib sources passes.
- Supervisor setup-failure/deadline regression test passes using a fake child, not a Linux guest.
- Full QEMU compilation/link and a second clean build using build-qemu.sh; QEMU 8.2.0 --version under Wasmer.
- Initial boot-tested debugless Wasm artifact: 12,690,524 bytes; debug-bearing: 36,245,818 bytes. After synchronous 9p adjustment, rebuild/version pass at 12,689,356 bytes debugless / 36,238,718 debug-bearing; this adjustment has not been boot-tested. No binaries committed.
- Six-path Lua closure, 12,514,847 compressed bytes / 43,686,496 NAR bytes, signature/hash/size verified and unpacked before execution. Directory artifact is not an immutable disk image.

## Failed or incomplete

512 MiB guest RAM, 768 MiB initial / 2 GiB maximum Wasm memory, 16 MiB TCI cache. Isolated verbose cold boot reaches SeaBIOS, Booting from ROM, and “Decompressing Linux...”. Process then exits SIGKILL after 133.540 seconds, host peak RSS 2,518,436 KiB. It never reaches the TryNix store handshake, shell, or Lua submission. The supervisor deadline was 600 seconds, so this was not its timeout. Source of the external SIGKILL was not established; do not label it confirmed OOM.

Original 2300 MiB initial shared-memory pthread configuration fails WASIX thread spawning with overflow, whereas a plain Wasm module of that size starts. Fixed initial=maximum also failed. These are runtime/ABI observations, not a universal Wasm size limit or Edge quota measurement.

sigsetjmp/siglongjmp escapes as an uncaught exception in a small probe. Plain setjmp/longjmp and a single jump across WASIX libffi pass under the default compiler. Only no-signal-mask QEMU jump sites are adapted.

### Repeated jumps expose host-memory growth

Independent tiny-module measurements, each in a fresh process:

| Compiler/path | Jumps | Peak host RSS, KiB | Seconds |
| --- | ---: | ---: | ---: |
| Cranelift/default | 100,000 | 116,808 | 0.45 |
| Cranelift/default | 1,000,000 | 506,036 | 3.40 |
| LLVM | 1,000,000 | 537,356 | 3.52 |
| -fno-exceptions module/default runtime | 1,000,000 | 592,936 | 3.98 |
| V8 | 1,000,000 | 84,340 | 1.45 |

Reported Wasm linear memory stays at 130 pages throughout. These are host RSS observations, not precise allocations attributable to each jump. V8 is not a solution: context_create returns ENOTSUP, and the libffi+longjmp probe traps with an uncaught exception. Ordinary repeated mixed-width libffi calls pass at 93,668 KiB, 0.21 seconds.

Wasmer source retains VMExceptionObj values in StoreObjects.exceptions. Each exception owns a payload and backtrace; reference counting is a TODO. This corroborates a known upstream limitation, rather than proving all QEMU RSS has one cause:

- https://github.com/wasmerio/wasmer/blob/v7.5.0/lib/vm/src/store.rs
- https://github.com/wasmerio/wasmer/blob/v7.5.0/lib/vm/src/exception.rs
- https://github.com/wasmerio/wasmer/discussions/6215 (Arshia001, February 17, 2026)

Minimal reproduction:

```sh
wasixcc -O2 ports/wasix/test-jump-stress.c -o jump.wasm
/usr/bin/time -v wasmer run --offline jump.wasm -- 100000
/usr/bin/time -v wasmer run --offline jump.wasm -- 1000000
# Compare separate fresh processes with --llvm or --v8.
```

Do not run this without an external memory/time limit in a constrained shared environment. No production runtime patch, unsafe exception recycling, full guest isolation proof, migration restore, browser-snapshot interchange or Edge compatibility is included. A RAM snapshot could skip decompression but would not bound subsequent exception growth.
