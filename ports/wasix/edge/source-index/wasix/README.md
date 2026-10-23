# WASIX libc / LLVM runtime provenance collection

Collected 2026-10-23 from official public repositories and the existing installed toolchain. This is provenance evidence, not legal clearance or a declaration that all redistribution materials are complete. Only this isolated directory was written. No software was installed or executed from these downloads; no binary asset was downloaded, no build/upload/deployment was performed, and runtime/PR files were not changed.

## Verified source relationships

- Official WASIX libc tag `v2026-10-02.1` resolves to commit `2025b44a5d2eb22b64ecdbf1bb16f3e7699dfb60`. The complete GitHub source archive for that commit is saved, SHA-256 `a0ada4fe6bfb8311a681acc09829e6256d1ef8ea9f902770e26732f8fafa4b48`.
- Its `.gitmodules` and API gitlink record `tools/llvm-project` at `6bb93a243f6d15855f485f5aec3810d9e2de150d`. The URL names `llvm/llvm-project`, but the commit is a patched WASM/libunwind commit, not an unmodified upstream release. `build32-general.sh` builds compiler-rt and C++ runtimes from this source and assembles them into the published sysroot. EH variants include libunwind.
- Its two interface submodules are `WebAssembly/WASI` at `bac366c8aeb69cacfea6c4c04a503191bf1cede1` and `wasix-org/wasix-witx` at `0dfbd35a0f30f3fe7fd3b3ab5a50dc4191d5caed`. Their complete source archives and available notices are saved.
- The separately installed compiler release `wasix-org/llvm-project` tag `21.1.206` resolves to `98c00cb849c94742b2b29918bb414d236c83ae97`. This is not the source revision of the sysroot runtime libraries.

## Source and notice coverage

`wasix-libc-selected/` copies build recipes and relevant notices from the saved source archive: root LICENSE, Apache/LLVM/Apache/MIT texts, musl COPYRIGHT, cloudlibc LICENSE, generator LICENSE files, and the original allocator source files containing their notices. The EH Makefile defaults to dlmalloc; emmalloc is retained for completeness, not asserted to be linked.

`llvm-runtime-selected/` contains full LICENSE.TXT files for compiler-rt, libc++, libc++abi, libunwind and LLVM at the exact runtime gitlink, plus available compiler-rt/libc++/libc++abi CREDITS.TXT. Preserve complete notices, including legacy license sections. Availability of C++/unwind archives in the sysroot does not establish that either final C Wasm links them.

The complete LLVM runtime source tree has NOT been downloaded. The exact corresponding source revision and official source URL are recorded, but this directory is not yet a self-contained source build bundle:
https://github.com/llvm/llvm-project/tree/6bb93a243f6d15855f485f5aec3810d9e2de150d

## Binary correspondence: evidence and limits

The existing `toolchain/install-wasixcc.log` identifies installation of `sysroot-exnref-eh.tar.gz` from release v2026-10-02.1 and `LLVM-Linux-x86_64.tar.gz` from compiler release 21.1.206. The saved official release API metadata supplies these published asset digests:

- `https://github.com/wasix-org/wasix-libc/releases/download/v2026-10-02.1/sysroot-exnref-eh.tar.gz`
  SHA-256 `c6fe6b4fd3e9576ed760c83a35f8d38e3d5f29e1b4d2df1af9d065d23278021a`
- `https://github.com/wasix-org/llvm-project/releases/download/21.1.206/LLVM-Linux-x86_64.tar.gz`
  SHA-256 `4066b52816c3382d10c5a7d7cfd6252a5f432459f007c143f07e23bf367c43ea`

Those digests are API-published evidence, NOT checksums recomputed from downloaded release binaries. `installed-exnref-runtime-hashes.json` fingerprints the existing local installed files independently, but no comparison to a release payload or reproducible rebuild was done. Build scripts plus the tag/gitlink establish the release's declared source relationship; they do not independently attest that the installed binary bytes were built from it. No final-Wasm linked-member map was generated.

The sysroot CI workflow uses `archlinux:base-devel` and installs floating `llvm21`/`clang21` packages. The exact CI container/compiler/package versions are not pinned in this source; the separately installed compiler release must not be substituted as proof of the compiler used to build the sysroot.

## Artifacts and next steps

`collection.json` records URLs, extraction origins, file sizes and independently computed hashes for every collected artifact. `SHA256SUMS` covers all stored artifacts, including this report. Remaining work before treating this as complete source correspondence: retain the exact LLVM source subtree/build dependencies, pin or recover the sysroot CI build environment, verify installed-vs-release binary correspondence as appropriate, map actual final-Wasm linked components, and integrate selected notices/materials into the distributable package. Existing unrelated guest and closure source gaps are outside this directory's scope.
