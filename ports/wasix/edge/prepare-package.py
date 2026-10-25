#!/usr/bin/env python3
"""Prepare a local-only Wasmer package; never uploads or deploys.

The input mapped share must be immutable while copying. Mapped-file guest
symlinks are regular files containing their targets, with mode/type stored in
.virtfs_metadata. Actual host symlinks are rejected, never dereferenced.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat


SHARE_ENTRIES = ("nix", "bin", "manifest", "closure-evidence.json")


def require_type(path, predicate, description):
    try:
        mode = path.lstat().st_mode
    except FileNotFoundError as error:
        raise ValueError(f"missing {description}: {path}") from error
    if not predicate(mode):
        raise ValueError(f"expected {description}, not a host symlink/special file: {path}")


def copy_regular(source, destination):
    """Hardlink an unchanged regular file, or exclusively copy across devices.

    Do not chmod/write a successful hardlink: that would change the source.
    The descriptor checks also reject a source replaced with a symlink between
    validation and copying. Input directory trees must not change concurrently.
    """
    descriptor = os.open(source, os.O_RDONLY | os.O_NONBLOCK |
                         getattr(os, "O_NOFOLLOW", 0))
    with os.fdopen(descriptor, "rb") as input_file:
        source_stat = os.fstat(input_file.fileno())
        if not stat.S_ISREG(source_stat.st_mode):
            raise ValueError(f"expected regular file: {source}")
        try:
            os.link(source, destination, follow_symlinks=False)
        except OSError:
            # Exclusive creation prevents overwriting an existing destination.
            with destination.open("xb") as output_file:
                shutil.copyfileobj(input_file, output_file)
                os.fchmod(output_file.fileno(), stat.S_IMODE(source_stat.st_mode))
        else:
            copied = destination.lstat()
            if (not stat.S_ISREG(copied.st_mode) or
                    (copied.st_dev, copied.st_ino) !=
                    (source_stat.st_dev, source_stat.st_ino)):
                destination.unlink()
                raise ValueError(f"source changed during hardlink: {source}")


def copy_mapped_tree(source, destination):
    mode = source.lstat().st_mode
    if stat.S_ISDIR(mode):
        destination.mkdir()
        for child in sorted(source.iterdir()):
            copy_mapped_tree(child, destination / child.name)
    elif stat.S_ISREG(mode):
        copy_regular(source, destination)
    else:
        raise ValueError(f"unexpected host symlink/special file in mapped share: {source}")


def validate_share(source):
    require_type(source, stat.S_ISDIR, "mapped share directory")
    require_type(source / ".virtfs_metadata", stat.S_ISDIR, "metadata directory")
    require_type(source / ".virtfs_metadata_root", stat.S_ISREG, "root metadata file")
    for name in SHARE_ENTRIES:
        expected = stat.S_ISDIR if name in ("nix", "bin") else stat.S_ISREG
        require_type(source / name, expected, f"mapped share entry {name}")
        require_type(source / ".virtfs_metadata" / name,
                     stat.S_ISREG, f"metadata record for {name}")


def copy_package_share(source, destination):
    """Copy only package runtime roots and their matching root metadata."""
    validate_share(source)
    destination.mkdir()
    copy_regular(source / ".virtfs_metadata_root", destination / ".virtfs_metadata_root")
    metadata = destination / ".virtfs_metadata"
    metadata.mkdir()
    for name in SHARE_ENTRIES:
        copy_mapped_tree(source / name, destination / name)
        copy_regular(source / ".virtfs_metadata" / name, metadata / name)


def main():
    parser = argparse.ArgumentParser()
    for name in ("supervisor", "qemu", "pack", "share", "out"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--package-name", default="local/trynix-lua-poc")
    args = parser.parse_args()
    source_share = args.share.resolve()
    output = args.out.resolve()
    if output == source_share or source_share in output.parents:
        parser.error("package output must be outside the source share")
    for path in (args.supervisor, args.qemu, args.pack / "machine.json"):
        if not path.is_file():
            parser.error(f"missing input: {path}")
    try:
        validate_share(source_share)
        machine_args = json.loads((args.pack / "machine.json").read_text())["args"]
        if not isinstance(machine_args, list) or not all(isinstance(x, str) for x in machine_args):
            raise ValueError("machine.json args must be a list of strings")
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))

    # New destination only. On failure, no manifest is emitted for a partial
    # copy. The original mapped share is never written or cleaned up.
    output.mkdir()
    (output / "emu").mkdir()
    for source, destination in (
        (args.supervisor.resolve(), output / "exec-api.wasm"),
        (args.qemu.resolve(), output / "emu/qemu-system-x86_64.wasm"),
    ):
        copy_regular(source, destination)
    copy_package_share(source_share, output / "share")

    command_args = ["/emu/qemu-system-x86_64.wasm"] + [
        value.replace("{pack}", "/pack").replace("{share}", "/share")
        .replace("{ram}", "512M").replace("tcg,tb-size=500", "tcg,tb-size=16")
        .replace("security_model=none", "security_model=mapped-file,readonly=on")
        for value in machine_args
    ]
    manifest = (
        "[package]\nname = " + json.dumps(args.package_name) + "\n"
        'version = "0.0.1"\n'
        'description = "Experimental QEMU Linux Lua POC"\n'
        'entrypoint = "serve"\n\n'
        '[[module]]\nname = "supervisor"\nsource = "exec-api.wasm"\n\n'
        '[[command]]\nname = "serve"\nmodule = "supervisor"\nrunner = "wasi"\n\n'
        "[command.annotations.wasi]\nmain-args = " + json.dumps(command_args) + "\n"
        'env = ["PUBLIC_POC=1", "PORT=80"]\n\n'
        '[fs]\n"/emu" = "emu"\n"/pack" = ' + json.dumps(str(args.pack.resolve())) + "\n"
        '"/share" = "share"\n'
    )
    (output / "wasmer.toml").write_text(manifest)
    (output / "NOT-FOR-PUBLISH.txt").write_text(
        "Local packaging only. Confirm approved free-quota exposure, Edge compatibility "
        "and complete redistribution source/license materials before uploading. "
        "No token, snapshot or persistent volume configured. Treat hardlinked package "
        "inputs as immutable; the original mapped share was not modified.\n"
    )
    print("Prepared local manifest and filtered share; build with wasmer package build. "
          "No upload or deployment performed.")


if __name__ == "__main__":
    main()
