# Binary/source distribution bundle plan

The tested runtime package is separate from this source collection to avoid mapping hundreds of MiB of compiler/kernel/emulator source into every runtime instance. Before binary publication, supply a companion source artifact from the same designated distribution location, clearly linked in the binary package README and source index, plus retain applicable notices inside the runtime package. Do not replace this with unrelated upstream links or an unapproved multi-year written offer.

The companion should contain:
- emulator archives, local port patch/build/postprocessing sources and configuration, and notices (emulator inventory);
- guest sources, pinned Nixpkgs source tree, exact kernel config, BusyBox/musl configuration-generation recipes and patches (guest inventory);
- all five Lua dependency source archives, declared patches and verified build derivations, plus Lua's shipped MIT notice (lua inventory);
- WASIX libc/interface sources, exact runtime revision/build recipe references and full applicable runtime notices (wasix inventory);
- source index, collection manifests and checksum index binding these materials to final supervisor/QEMU/guest/package hashes.

Do not silently claim a source URL is accessible at the distribution destination before upload/read-back verifies it. Registry/source-package upload and final source delivery remain unperformed. Historical binary reproducibility caveats are documented by component and do not independently impose a universal rebuild requirement.

The runtime build statically links LGPL GLib; full application source/build/relink materials and license permissions need to accompany the offer rather than only GLib's license text. The port build sources are available under the existing draft PR. No custom license terms restricting relinking are added.

This is an engineering checklist based on bundled primary license texts, not legal clearance. No written source offer or multi-year fulfillment commitment is made.
