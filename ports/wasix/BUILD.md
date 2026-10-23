# Development build

Use a separate workspace. Do not apply these patches to the browser QEMU build.
Requirements: official wasixcc 0.4.7, Wasmer 7.5.0, Meson 1.5.0, Ninja, Python 3 with pyelftools and distlib, pkg-config, GNU Autoconf/Automake/Libtool, make, patch, and Node 24 for the closure builder. Keep toolchain installation isolated; no account credentials or new network grants are requested at runtime. Wasmer --offline is not a network sandbox: isolate networking externally when denial is required, especially if the Wasmer home directory has cached permissions.

Sources and archive SHA-256 values are recorded in pins.json. Fetch from the original projects:

- https://github.com/ktock/qemu-wasm/archive/0ef7b4e2814b231705d8371dd7997f5b72e70baf.tar.gz
- https://download.qemu.org/qemu-8.2.0.tar.xz
- https://github.com/wasix-org/libffi/archive/09cbf7d66d232a01dbb0c88fd5ae65fa9c15f7c7.tar.gz
- https://download.gnome.org/sources/glib/2.75/glib-2.75.0.tar.xz
- https://zlib.net/fossils/zlib-1.3.1.tar.xz

Verify archive checksums before extracting. The GitHub QEMU archive lacks submodule sources: copy subprojects/dtc, subprojects/keycodemapdb, subprojects/berkeley-softfloat-3 and subprojects/berkeley-testfloat-3 from the official QEMU 8.2.0 archive into the fork. Do not apply the TryNix Emscripten-specific patches to this WASIX target.

Set PORT to the absolute path of ports/wasix. On clean extracted sources:

```sh
(cd "$LIBFFI_SOURCE" && patch -p1 < "$PORT/libffi-wasix.patch")
(cd "$GLIB_SOURCE" && patch -p1 < "$PORT/glib-minimal.patch")
(cd "$QEMU_SOURCE" && patch -p1 < "$PORT/qemu-wasix.patch")
(cd "$QEMU_SOURCE" && patch -p1 < "$PORT/qemu-wasix-runtime.patch")
cp "$PORT/coroutine-wasix.c" "$QEMU_SOURCE/util/coroutine-wasix.c"
cp "$PORT/9p-errno-wasix.inc" "$QEMU_SOURCE/hw/9pfs/9p-errno-wasix.inc"
bash "$PORT/build-deps.sh" "$LIBFFI_SOURCE" "$ZLIB_SOURCE" "$GLIB_SOURCE" "$DEPS_PREFIX" "$DEPS_BUILD"
bash "$PORT/build-qemu.sh" "$QEMU_SOURCE" "$DEPS_PREFIX" "$QEMU_BUILD"
wasmer run --offline "$QEMU_BUILD/qemu-system-x86_64.wasm" -- --version
```

The dependency script passed an end-to-end replay on pristine pinned dependency sources. The QEMU script also passed a fresh build using the initial static dependencies. Meson may fetch its pinned PCRE2 10.40 fallback while building GLib. No downloaded binaries are committed here.

For probes: python3 ports/wasix/test-capabilities.py. For cold boot, fetch machine.json, bzImage, initramfs.cpio.gz and the four BIOS blobs listed in pins.json from https://trynix.dev/guest/, verify every hash, and put them in PACK. Prepare a new isolated staging directory with `node ports/wasix/prepare-lua.mjs STAGING`, then convert it with `python3 ports/wasix/prepare-mapped-share.py STAGING SHARE`. Both steps are build-time; do not pass the plain staging directory to the mapped-file runner. Then:

```sh
python3 ports/wasix/run-guest.py "$QEMU_BUILD/qemu-system-x86_64.wasm" "$PACK" "$SHARE" --verbose-kernel --timeout 300
```

This gate passed locally; see RESULTS.md. It kills the process group after success or deadline and limits serial output and RSS (1.8 GiB). The measured pass used the same source changes and postprocessing; the packaged combined build-script replay now passes from pristine pinned sources; the final clean-artifact guest replay also passed Linux boot and Lua exit0. Use an externally constrained development environment. Guest 9p is read-only by default, but host volume mappings remain writable: use only disposable dedicated directories, never a host root or shared user files. Kernel/BIOS licensing and QEMU/GLib/libffi source obligations still apply if redistributing artifacts.

The runtime patch is applied after the base patch. Binaryen wasm-dis/wasm-as from the pinned wasixcc installation are used for the bounded longjmp rewrite; override WASM_DIS/WASM_AS if needed. Do not enable all Wasm proposals blindly: compact imports are not supported by this Wasmer version. Plugins and non-TCI accelerators must stay disabled. The legacy CONFIG_WASIX_ASYNCIFY flag enables typed helpers only, not Asyncify.

Lightweight packaging regressions (no guest boot): `python3 ports/wasix/test-runner.py`, `python3 ports/wasix/test-mapped-share.py`, and `python3 ports/wasix/test-cache-transform.py`. The generated-helper ABI test is `bash ports/wasix/test-generated-thunks.sh "$QEMU_SOURCE" "$DEPS_PREFIX" "$TEST_OUTPUT"`.

Final-guest lifecycle sanity gate (creates a new output directory): `python3 ports/wasix/test-guest-lifecycle.py WASM PACK MAPPED_SHARE NEW_OUTPUT`. It runs two sequential guests, checks warm reuse and guest-read-only export, cancels busy Lua, then checks fresh guest RAM state. It is not a malicious-guest isolation test.
