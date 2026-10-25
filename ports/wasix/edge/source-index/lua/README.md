# Lua closure source collection

The six shipped outputs are identified by signed NAR evidence in the mapped share. Five component derivations were available from cache.nixos.org. Their signatures and NAR hashes were verified, and their declared outputs match the shipped ncurses, glibc, libgcc, libidn2 and libunistring identities. Source-fetch derivations declare exact archive hashes; every collected archive matches. All declared glibc/GCC patches were also fetched and verified through signed NARs. collection.json retains derivations, output identities, URLs and hashes. No rebuild was performed.

Lua's own derivation returned HTTP404. Its exact MIT notice is retained from the shipped include/lua.h; the pinned guest nixpkgs Lua recipe is5.5.0 and must not be represented as the shipped5.5.1 recipe. MIT requires its copyright/permission notice, not a corresponding-source offer; unavailable .drv is a reproducibility limitation rather than an independent MIT source obligation. https://www.lua.org/license.html

## Redistribution checklist, not legal assurance

- Preserve Lua MIT and ncurses copyright/permission/disclaimer notices.
- Ship applicable glibc LGPL text and its LICENSES aggregate notices. The dynamically shipped library itself needs corresponding-source access under its applicable license, not merely a runtime-link exception. Source, all11 patches and derivation build attributes are collected.
- libidn2/libunistring sources contain GPL/LGPL alternatives and additional notices. Preserve all applicable notices and corresponding source/build materials; consult the retained README/license texts for component selection. Do not infer a single license from top-level COPYING alone.
- libgcc carries GPLv3 plus GCC Runtime Library Exception3.1. Preserve both. The exception concerns eligible compiled combinations and is not a blanket reason to omit this separately distributed library's source. Complete GCC archive, declared six patches and build derivation are retained.
- Keep dynamic library replacement/relinking possible in redistributed material. Read-only execution mounts are an operational sandbox setting, not a license restriction on recipients changing and rebuilding their own copy. Do not add terms forbidding modification/reverse engineering for debugging those changes.
- For covered binaries offered for copying, attach corresponding sources/recipes/patches or arrange the license's applicable equivalent-source-access option at the distribution point. Ordinary upstream archive links omit our collected distro patches and build attributes. Do not substitute a written multi-year source offer without someone accepting that ongoing obligation.

Full historical build environments and byte-identical reproduction are not asserted. Inputs in .drv records include build tools; not every historical tool has been recursively mirrored. This is an engineering source/notices inventory, not legal clearance or proof of hostile-code isolation.
