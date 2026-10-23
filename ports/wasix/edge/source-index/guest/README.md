# Guest source and notice collection

Collected 2026-10-23 for the existing TryNix WASIX local package. This is an engineering inventory, not legal clearance. No build, installation, guest execution, upload or deployment was performed.

## Result

The listed guest source inputs are no longer missing. `source-inputs.json` verifies all 15 pinned archives/remote patches/auxiliary source files. `component-manifest.json` maps Linux, BusyBox, static musl and the initramfs to retained source, recipes, notices and bounded uncertainty.

- Linux 7.2.5: exact kernel.org archive SHA-256 verified; complete license tree retained; actual embedded kernel configuration recovered as `inspection/linux-7.2.5.embedded.config`. Pinned recipe declares no kernel source patches.
- BusyBox 1.37.0: exact archive and all four normalized remote patches verified. Five in-tree patches, including the conditional cross-build patch, are present in the verified Nixpkgs source. Configuration-generation and compilation/installation instructions are retained, not just the unpatched upstream archive.
- musl 1.2.6: exact source archive, three patches and all five auxiliary source/header inputs verified. Its complete COPYRIGHT notice is retained. This version follows the pinned package recipe; the exact libc objects within the static binaries were not independently attested.
- Initramfs: 405 cpio entries, exactly three regular files (`bin/busybox`, `bin/reseed`, `init`). `init` matches the pinned original source byte for byte. Init/reseed source and deterministic archive-assembly recipe are retained with the TryNix MIT notice.

## Matching evidence

`recipes/engine-pins.json` is from TryNix commit `2da7f8ac0bdd922ff29b17c8327c00c1d39e1355`. Its bzImage, initramfs and machine.json hashes match the actual guest files exactly. This connects the files to the upstream pinned guest recipe; it is not merely a locally authored manifest assertion.

The official Nixpkgs source archive for `0968519e14f7aa7d3e9b389682bd74d2b51c8ce8` reproduces the flake.lock NAR hash `sha256-lhEhY8X5EgkQ/eg6IFz4cc8jRuSYSvnxx1al7d1dvZ0=`. `verify_nixpkgs_nar.py` implements canonical NAR serialization without installing or evaluating Nix.

The BusyBox executable also matches byte-for-byte the signed `cache.nixos.org` output `/nix/store/k2wdqr6nyq0pkx1qbk04k0b2dhcddcdx-busybox-static-x86_64-unknown-linux-musl-1.37.0`. Its NAR signature and hash were verified. Narinfo `Deriver` is not covered by the Nix output-signature fingerprint and is not treated as authenticated source correspondence by itself.

## Bounded uncertainty, not an endless reproducibility gate

- The original BusyBox .drv was unavailable from cache.nixos.org. No `bbconfig` applet/symbol or embedded configuration was recovered. Both standard compressed payloads were decoded as data and contain no configuration. The pinned `defconfig` plus configuration overrides and all build scripts are available; the original generated .config is not.
- musl version/patch correspondence and reseed's exact build follow the pinned source recipe and matching upstream guest hashes; no byte-identical rebuild, link map or original musl derivation was obtained.
- GCC 15.3.0 and GNU ld 2.46 are observed in the kernel, and GCC 15.3.0 in the two static ELF files. A complete independently reproduced compiler toolchain is not claimed. Missing .drv metadata or a bit-identical rebuild is not, by itself, a requirement stated in the bundled GPLv2 license.
- The full pinned Nixpkgs tree preserves build scripts and dependency recipes. Not every non-shipped host build tool's source or the Debian udhcpc dispatcher source tree was downloaded separately. The dispatcher is absent from the initramfs; only its configured store path appears in BusyBox.

These caveats must not be converted into claims that the binary used unmodified vanilla BusyBox, that the exact static libc was independently proven, or that any arbitrary upstream version is a matching replacement.

## Concrete source/notice delivery work remaining

The materials above are local only. They have not been put into a public package or made available to its recipients. Include the component notices, relevant complete sources, patches, configuration/build/installation scripts and an index in the chosen binary distribution/source offering. Generic links to vanilla Linux or BusyBox alone do not convey the Nixpkgs changes and guest configuration.

The bundled primary licenses say:

1. GPLv2 section 3(a): accompany distributed object/executable code with complete corresponding machine-readable source under sections 1 and 2. The same section defines executable source to include source for the modules it contains, associated interface definitions, and compilation/installation control scripts, subject to its stated system-component exception.
2. GPLv2 section 3's final paragraph: if binaries are offered for copying from a designated place, equivalent access to copy the source from that same place counts. Thus a source artifact/source download offered alongside the binary at the chosen distribution destination is the straightforward engineering route. Do not treat unrelated external-project links alone as verified satisfaction of this same-place option.
3. GPLv2 section 3(b): an alternative is an accompanying written offer valid for at least three years, available to any third party, at no more than the physical source-distribution cost. That is a real continuing commitment, not an automatically generated sentence. No offer was made here.
4. GPLv2 section 3(c) permits passing on received source-offer information only in its limited noncommercial situation where the binary was received with a qualifying offer. No qualifying received offer was established here.
5. GPLv2 sections 1/2 also address preserving copyright/warranty/license notices and marking modifications. Linux's COPYING specifies GPLv2-only with its syscall exception, plus applicable per-file licensing. BusyBox's bundled license and all upstream source notices are preserved.
6. musl and TryNix MIT notices require retaining the applicable copyright and permission notices. MIT itself does not impose a source-distribution or exact-rebuild requirement. Including the retained musl sources conservatively helps document the static combined executable; it does not prove the inferred version independently.

Primary texts: `notices/busybox/LICENSE` section 3 (lines 142–178); `notices/linux/LICENSES/preferred/GPL-2.0` section 3 (lines 153–189); `notices/linux/COPYING`; `notices/linux/LICENSES/exceptions/Linux-syscall-note`; `notices/musl/COPYRIGHT`; `notices/TryNix-LICENSE`. These are source-derived texts, not legal advice about a particular deployment's distribution boundaries.

## Retrieval and rechecking

Official BusyBox/musl downloads returned HTTP 503 during collection. Their exact pinned bytes and normalized BusyBox patches were recovered from signed NixOS cache source outputs and checked again against the recipe's content hashes. The failed initial attempts in `download-manifest.json` and `mirror-manifest.json` are superseded for availability by the successful `cache-input-manifest.json` and aggregate `source-inputs.json`; nothing was silently substituted.

- `inspect_guest.py`: parses cpio/ELF/kernel compressed data without executing guest binaries.
- `verify-busybox.mjs`: verifies the signed cached output and compares the shipped BusyBox bytes.
- `verify_nixpkgs_nar.py`: verifies the source tree against the locked NAR hash.
- `finalize_manifest.py`: checks all 15 pinned inputs and writes the aggregate/component manifests.
- `SHA256SUMS`: locally retained file integrity, excluding itself, generated integrity logs, Python bytecode and the transient decompressed-Nixpkgs verification cache.

Source identity URLs are in `source-inputs.json`; all needed main archives are under `archives/`. The firmware/QEMU/WASIX and Lua closure inventories are outside this guest subtask.
