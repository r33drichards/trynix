# Current milestone: local Linux + Lua PASS

2026-10-23, Wasmer7.5.0/wasixcc0.4.7, default Cranelift backend:

- Cold guest boot, init handshake, Lua hello marker and exit0: **PASS**,63.519seconds.
- Wasm12,630,670bytes, SHA256`cf295debf9910fd74169b77bc8f1179613ed545786c1db806b3a7ef41e50ef6a`.
- Peak host RSS860,888KiB; guest512MiB; TCIcache16MiB; linear-memory initial768MiB/max2GiB.
- Runner deliberately killed process group after success. Returncode-9 records cleanup, not guest failure.
- Exact generated helper-macro ABI harness:15cases, ordinary EH1,000jumps passed; cached1,000,000jumps passed in2.122s at81,764KiB. Covers arities0–7, signed values, Int128, pointers, softfloat bits, overlapping TCI-style slots, memset, and longjmp.
- Normal host libffi calls grew from213,364KiB at1million to742,844KiB at5million. Direct typed Wasm function calls at5million stayed80,032KiB. Source table lookup allocates retained VMFunction store handles. The new QEMU module has zero call_dynamic imports.
- Cached typed-helper QEMU first completed Linux decompression but failed at APIC/timer setup: Cranelift SIGILL, LLVM out-of-bounds function-table access. LLVM trace mapped to QEMU sigfd_handler. Tiny sigwait returned success/signal0 before sender fired. Removing the unsupported host signal-to-fd bridge enabled complete Linux boot.
- Lua initially failed exit126: WASIX stat reported100000 instead of host100755. Existing QEMU mapped-file metadata, with converted symlink representation, preserved executable bits and enabled Lua exit0.

Lifecycle sanity matrix also passed: second Lua marker0.407s after first; guest export rejected writes; busy Lua cancellation removed the process group in0.008s; fresh-after-cancel guest booted in23.469s with no prior /tmp canary, ran Lua and cleaned up in0.016s. Cumulative child peak655,896KiB across these sequential runs. See evidence/lifecycle.json and lifecycle-serial.txt. This is not hostile escape-resistance testing.

Clean packaged combined build replay passed on pristine pinned QEMU sources and freshly replayed dependencies: 1,512 build steps, version 8.2.0 under Wasmer, 12,631,774-byte processed artifact (SHA256 ae38c921b6d0b9635c4de494b1fcad7eca0a98ed0c397aec25c3c2e593b40517). All 38 patched/copied source files matched the passing experimental source. The generated ABI driver also passed on this clean source, including one million cached jumps. Final clean-artifact guest replay also PASSED: Linux handshake and Lua exit0 in 23.997 seconds, child peak RSS 746,560 KiB (periodic /proc sample maximum 747,104 KiB). This run reused the host compilation cache but booted a fresh Linux guest; it is not a fresh-host cold-cache latency measurement. See evidence/clean-replay.json and clean-replay-serial.txt. QEMU migration and Wasmer store snapshot/restore are not validated; the exception cache explicitly forbids Wasmer store transfer/restore. No Edge deployment or quota verification.

The following sections are historical baseline results before typed helpers, bounded exception caching, host-signal-contract correction and mapped-file metadata. Their earlier “not booted” statements do not describe the current milestone.

---

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


## Coroutine C-stack isolation gate (2026-10-24, local Wasmer 7.5.0)

`ports/wasix/coroutine-wasix.c` gave every coroutine the shared module-level guest C stack: WASIX switches the engine call stack only, while `__stack_pointer` is per-store. N concurrently suspended coroutines therefore consume one stack region. `test-coroutine-stack.c` drives N coroutines through the real backend (16 KiB address-taken frame + 256-byte guard each, all suspended before any resume).

| N | backend before | backend with per-coroutine stack + `coroutine-wasix-switch.S` |
|---|---|---|
| 8 | ok, used span 132,864 B | ok, 0 bad guard bytes |
| 64 | ok, span 1,064,704 B | ok, 0 bad guard bytes |
| 256 | ok, span 4,259,584 B | ok, 0 bad guard bytes |
| 512 | traps: `RuntimeError: out of bounds memory access` at `worker_entry`/`coroutine_trampoline` | ok, 0 bad guard bytes |
| 700 | traps | ok, 0 bad guard bytes |
| 1500 | - | ok, 0 bad guard bytes |

The shim saves the caller `__stack_pointer`, installs the target's saved pointer, performs `wasix_context_switch`, and restores on resume; it is frameless and touches no C locals, which is safe in Wasm because return addresses live in the engine call stack rather than linear memory. Each coroutine owns a bounded 256 KiB region (`COROUTINE_WASIX_STACK_SIZE`).

Not established here: byte-level clobbering of a suspended coroutine's frames did not reproduce in two-context probes on this toolchain (0 mismatches every run); no full QEMU build was run, so the `util/meson.build` wiring for the `.S` is unverified; and with `--disable-coroutine-pool` create/delete now costs a 256 KiB allocation per coroutine. Linux boot, snapshot/restore, lifecycle, isolation and Edge remain untested by this gate. Machine-readable results: `evidence/coroutine-stack.json`.
