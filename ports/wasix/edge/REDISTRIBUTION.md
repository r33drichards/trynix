# Binary package provenance checkpoint

The draft PR publishes source and test evidence, not compiled binaries. Local .webc packaging/execution passed, but the local container is not yet a complete redistribution/source-notice bundle. This is an engineering inventory, not legal clearance.

## Available materials

- QEMU fork/upstream archives, exact commit, runtime patches, build/postprocessing scripts and source licenses.
- GLib2.75.0, libffi09cbf7d (3.5.1), zlib1.3.1, PCRE2 10.40 and libfdt source/notices. The final QEMU link includes PCRE2 and libfdt; do not omit them from the inventory. PCRE2 source and Meson patch archive checksums are recorded in pins.json.
- SeaBIOS source and GPL/LGPL notices are present inside the upstream QEMU8.2.0 archive; packaged BIOS blobs match that archive. QEMU pc-bios/optionrom includes linuxboot/kvmvapic sources.
- Pinned TryNix kernel recipe/configs, guest init and reseed source. Lua signed NAR verification evidence and Lua MIT notice in include/lua.h.

## Gaps before treating registry packaging as complete

1. Add TryNix's MIT notice, applicable third-party notices and a source index mapping every shipped binary to exact source/configuration/patch/build materials. The local manifest currently has no complete notices bundle.
2. Resolve exact linked WASIX libc/compiler-rt source revisions and notices for both supervisor and QEMU. Observed installer release URLs are not verified source-commit correspondence; wasixcc wrapper version alone is not a complete toolchain pin.
3. Collect matching guest Linux source, BusyBox1.37.0 source/config/patches and static musl/runtime inputs. Pinned nixpkgs revision0968519e14f7aa7d3e9b389682bd74d2b51c8ce8 is recorded in the repository, but its source tree and these source archives were not collected in the passing workspace.
4. Retain source derivation mappings and notices for all six mapped closure outputs: Lua5.5.1, glibc2.44-57, ncurses6.6, libgcc16.2.0, libidn2 2.3.8 and libunistring1.4.2. The existing prepare-lua.mjs parser discards Narinfo Deriver; signed binary provenance does not establish corresponding source.
5. Bind final package/supervisor/QEMU hashes and port revision to that inventory. Preserve rebuild/relink inputs, not only generic upstream links. No assertion of complete redistribution compliance is made here.

No source downloads, registry uploads, account grants or deployments were performed for this read-only audit.
