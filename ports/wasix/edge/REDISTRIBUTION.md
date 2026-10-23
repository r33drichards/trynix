# Binary/source redistribution checkpoint

The draft PR publishes source, source-collection indexes and test evidence, not compiled binaries or large upstream archives. The passing local .webc is not yet accompanied by a published source/notices offering. This is an engineering inventory, not legal clearance.

## Resolved locally

- QEMU fork/upstream, GLib, libffi, zlib, PCRE2, libfdt and firmware source archives; actual port patches/build/postprocessing sources are retained. Static GLib relinking requires usable application source/build materials, not just a license text.
- Guest Linux7.2.5, BusyBox1.37.0 and musl1.2.6 sources, pinned Nixpkgs tree, configuration-generation recipes, kernel embedded configuration and all applicable pinned patches/headers are collected. All15 external inputs match hashes. Guest artifacts match upstream TryNix pins; BusyBox bytes match signed NixOS cache output.
- WASIX libc revision2025b44a5d2eb22b64ecdbf1bb16f3e7699dfb60 and interface sources, LLVM runtime revision6bb93a243f6d15855f485f5aec3810d9e2de150d and component notices are resolved. Installed compiler is a separate revision. Full LLVM source tree was not mirrored; retained exact source references/notices and license requirements must not be confused with proving a historical floating CI environment.
- Five Lua closure dependencies have signed-cache derivations declaring the shipped output identities. Every source archive matches its source-fetch derivation hash, and all glibc/GCC patches are retained. Lua's exact MIT copyright/permission notice comes from shipped lua.h.

## Caveats versus actual delivery gates

Lua's own .drv and BusyBox's original .drv return404. BusyBox generated .config is not embedded; its locked configuration-generation/build recipe is retained. Musl linkage is recipe-inferred. These are documented provenance/reproducibility caveats, not automatic requirements to reproduce every historical compiler or obtain a bit-identical rebuild. Lua MIT requires its notice, not a source offer.

Before binary publication, include applicable notices and provide corresponding-source/configuration/patch/build materials under the relevant GPL/LGPL options. Generic upstream links alone omit distribution patches and configuration. The planned companion source artifact must be offered alongside the binary at the same distribution destination and verified accessible. No multi-year written offer is made or silently imposed. See source-index/BUNDLE-PLAN.md and component indexes; indexed local source paths are collection-relative, not published downloads.

Bind final source inventory to evidence/persistent-artifacts.json, final source revision and any subsequent final package hash. Preserve usable rebuild/relink materials and recipients' applicable modification rights. Source retrieval has been performed; no registry upload, token creation or Edge deployment has occurred.
