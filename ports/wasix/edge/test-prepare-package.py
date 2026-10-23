#!/usr/bin/env python3
"""Tiny local fixtures for package preparation; no build, guest or upload.

Run with python3 ports/wasix/edge/test-prepare-package.py. The fake WASM
inputs are only bytes to copy, never executable modules. Every write stays
in a disposable temporary directory, including forced copy-fallback tests.
"""
import contextlib
import errno
import io
import json
import os
from pathlib import Path
import runpy
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("prepare-package.py").resolve()
SHARE_ENTRIES = {"nix", "bin", "manifest", "closure-evidence.json"}


def snapshot(root):
    """Contents, types, permission bits and mtimes, not hardlink counts/ctime."""
    result = {}
    for path in [root, *sorted(root.rglob("*"))]:
        info = path.lstat()
        if stat.S_ISREG(info.st_mode):
            payload = path.read_bytes()
        elif stat.S_ISLNK(info.st_mode):
            payload = os.readlink(path)
        else:
            payload = None
        result[str(path.relative_to(root))] = (
            info.st_mode, info.st_mtime_ns, payload
        )
    return result


class PreparePackageTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="prepare-package-test-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.supervisor = self.root / "supervisor.wasm"
        self.qemu = self.root / "qemu.wasm"
        self.supervisor.write_bytes(b"fake supervisor, never executed\n")
        self.qemu.write_bytes(b"fake QEMU, never executed\n")
        self.supervisor.chmod(0o751)
        self.qemu.chmod(0o640)
        self.pack = self.root / "pack"
        self.pack.mkdir()
        (self.pack / "machine.json").write_text(json.dumps({"args": [
            "-kernel", "{pack}/kernel", "-m", "{ram}",
            "-accel", "tcg,tb-size=500",
            "-virtfs", "local,path={share},security_model=none,mount_tag=host0",
        ]}))
        self.share = self.root / "mapped-share"
        self.share.mkdir()
        self.write_root_metadata(self.share)
        self.mapped_directory("nix/store/tiny-lua/bin")
        self.mapped_directory("nix/store/tiny-lua/lib")
        self.mapped_file("nix/store/tiny-lua/bin/lua", b"tiny executable\x00\n",
                         stat.S_IFREG | 0o755, host_mode=0o640)
        self.mapped_file("nix/store/tiny-lua/lib/runtime.so", b"tiny library\n",
                         stat.S_IFREG | 0o644, host_mode=0o444)
        self.mapped_directory("bin")
        # mapped-file encodes guest symlinks as ordinary host files. Their
        # payload and metadata must survive; following them would be wrong.
        self.mapped_file("bin/lua", b"/nix/store/tiny-lua/bin/lua",
                         stat.S_IFLNK | 0o777, host_mode=0o600)
        self.mapped_file("nix/store/tiny-lua/lib/current", b"runtime.so",
                         stat.S_IFLNK | 0o777, host_mode=0o640)
        self.mapped_file("manifest", b"/nix/store/tiny-lua\n", stat.S_IFREG | 0o644)
        self.mapped_file("closure-evidence.json", b'{"fixture": true}\n',
                         stat.S_IFREG | 0o644)
        self.mapped_directory("logs")
        self.mapped_file("logs/run.log", b"excluded nested log\n", stat.S_IFREG | 0o600)
        self.mapped_file("boot.log", b"excluded root log\n", stat.S_IFREG | 0o600)
        for name in ("baseline-serial.log", "cached-serial.log"):
            self.mapped_file(name, b"original leaked-root regression fixture\n",
                             stat.S_IFREG | 0o600)
        self.mapped_file("notes.txt", b"not a runtime input\n", stat.S_IFREG | 0o644)
        self.output = self.root / "package"

    @staticmethod
    def metadata_bytes(mode):
        return f"virtfs.uid=0\nvirtfs.gid=0\nvirtfs.mode={mode}\nvirtfs.rdev=0\n".encode()

    def write_root_metadata(self, directory):
        (directory / ".virtfs_metadata_root").write_bytes(
            self.metadata_bytes(stat.S_IFDIR | 0o755)
        )

    def write_entry_metadata(self, path, mode):
        metadata = path.parent / ".virtfs_metadata"
        metadata.mkdir(exist_ok=True)
        (metadata / path.name).write_bytes(self.metadata_bytes(mode))

    def mapped_directory(self, relative):
        current = self.share
        for component in Path(relative).parts:
            current = current / component
            if not current.exists():
                current.mkdir()
                self.write_root_metadata(current)
                self.write_entry_metadata(current, stat.S_IFDIR | 0o755)

    def mapped_file(self, relative, payload, guest_mode, host_mode=0o644):
        path = self.share / relative
        path.write_bytes(payload)
        path.chmod(host_mode)
        self.write_entry_metadata(path, guest_mode)

    def argv(self, output=None):
        return [str(SCRIPT), str(self.supervisor), str(self.qemu), str(self.pack),
                str(self.share), str(self.output if output is None else output)]

    def prepare(self, output=None):
        return subprocess.run([sys.executable, *self.argv(output)],
                              capture_output=True, text=True, timeout=15)

    def input_snapshot(self):
        return {"share": snapshot(self.share), "pack": snapshot(self.pack),
                "supervisor": snapshot(self.supervisor), "qemu": snapshot(self.qemu)}

    def staged_files(self):
        yield self.supervisor, self.output / "exec-api.wasm"
        yield self.qemu, self.output / "emu/qemu-system-x86_64.wasm"
        for source in self.share.rglob("*"):
            relative = source.relative_to(self.share)
            selected = (relative.parts[0] in SHARE_ENTRIES or
                        relative == Path(".virtfs_metadata_root") or
                        (len(relative.parts) == 2 and
                         relative.parts[0] == ".virtfs_metadata" and
                         relative.parts[1] in SHARE_ENTRIES))
            if selected and source.is_file():
                yield source, self.output / "share" / relative

    def assert_staged_share(self):
        staged = self.output / "share"
        self.assertEqual({path.name for path in staged.iterdir()},
                         SHARE_ENTRIES | {".virtfs_metadata", ".virtfs_metadata_root"})
        self.assertEqual({path.name for path in (staged / ".virtfs_metadata").iterdir()},
                         SHARE_ENTRIES)
        for name in SHARE_ENTRIES:
            source = self.share / name
            destination = staged / name
            if source.is_dir():
                self.assertEqual(set(snapshot(source)), set(snapshot(destination)))
        for source, destination in self.staged_files():
            with self.subTest(staged=str(destination.relative_to(self.output))):
                self.assertFalse(destination.is_symlink())
                self.assertEqual(destination.read_bytes(), source.read_bytes())
                self.assertEqual(stat.S_IMODE(destination.stat().st_mode),
                                 stat.S_IMODE(source.stat().st_mode))
        for relative in ("bin/lua", "nix/store/tiny-lua/lib/current"):
            path = staged / relative
            record = path.parent / ".virtfs_metadata" / path.name
            fields = dict(line.split("=", 1) for line in record.read_text().splitlines())
            self.assertEqual(int(fields["virtfs.mode"]), stat.S_IFLNK | 0o777)
            self.assertTrue(stat.S_ISREG(path.lstat().st_mode))

    def test_allowlisted_share_metadata_and_source_unchanged(self):
        before = self.input_snapshot()
        result = self.prepare()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_staged_share()
        self.assertEqual(self.input_snapshot(), before)
        manifest = (self.output / "wasmer.toml").read_text()
        self.assertIn('"/share" = "share"\n', manifest)
        self.assertNotIn(str(self.share.resolve()), manifest)
        self.assertIn("security_model=mapped-file,readonly=on", manifest)
        self.assertIn("512M", manifest)
        self.assertIn("tcg,tb-size=16", manifest)
        warning = (self.output / "NOT-FOR-PUBLISH.txt").read_text().lower()
        self.assertNotIn("expiry", warning)
        self.assertIn("quota", warning)
        self.assertIn("edge", warning)
        self.assertIn("source", warning)
        self.assertIn("license", warning)

    def test_regular_files_are_hardlinked_where_supported(self):
        probe = self.root / "hardlink-probe"
        try:
            os.link(self.supervisor, probe)
        except OSError as error:
            self.skipTest(f"fixture filesystem does not support hardlinks: {error}")
        probe.unlink()
        result = self.prepare()
        self.assertEqual(result.returncode, 0, result.stderr)
        for source, destination in self.staged_files():
            with self.subTest(staged=str(destination.relative_to(self.output))):
                self.assertTrue(os.path.samefile(source, destination))

    def test_copy_fallback_preserves_bytes_modes_and_inputs(self):
        before = self.input_snapshot()
        # Inject a cross-device failure without editing the script or copying
        # actual artifacts between filesystems. All regular files take fallback.
        with mock.patch.object(sys, "argv", self.argv()), \
                mock.patch("os.link", side_effect=OSError(errno.EXDEV, "fixture")) as link, \
                contextlib.redirect_stdout(io.StringIO()):
            runpy.run_path(str(SCRIPT), run_name="__main__")
        self.assertGreater(link.call_count, 2)
        self.assert_staged_share()
        self.assertEqual(self.input_snapshot(), before)
        for source, destination in self.staged_files():
            self.assertFalse(os.path.samefile(source, destination))

    def test_output_within_share_is_rejected_before_any_writes(self):
        alias = self.root / "share-alias"
        alias.symlink_to(self.share, target_is_directory=True)
        for output in (self.share, self.share / "package", self.share / "nix/package",
                       alias / "package"):
            with self.subTest(output=str(output)):
                before = self.input_snapshot()
                result = self.prepare(output)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("outside", result.stderr.lower())
                self.assertEqual(self.input_snapshot(), before)

    def test_missing_required_root_metadata_is_rejected(self):
        paths = [self.share / ".virtfs_metadata_root", self.share / ".virtfs_metadata"] + [
            self.share / ".virtfs_metadata" / name for name in sorted(SHARE_ENTRIES)
        ]
        for index, path in enumerate(paths):
            with self.subTest(metadata=str(path.relative_to(self.share))):
                saved = self.root / "temporarily-removed-metadata"
                path.rename(saved)
                before = self.input_snapshot()
                output = self.root / f"missing-metadata-{index}"
                result = self.prepare(output)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("metadata", result.stderr.lower())
                self.assertFalse(output.exists())
                self.assertEqual(self.input_snapshot(), before)
                saved.rename(path)

    def test_missing_runtime_entries_are_rejected(self):
        for index, name in enumerate(sorted(SHARE_ENTRIES)):
            with self.subTest(entry=name):
                source = self.share / name
                saved = self.root / "temporarily-removed"
                source.rename(saved)
                before = self.input_snapshot()
                output = self.root / f"missing-entry-{index}"
                result = self.prepare(output)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(output.exists())
                self.assertEqual(self.input_snapshot(), before)
                saved.rename(source)

    def test_actual_host_symlinks_are_rejected_without_following(self):
        external = self.root / "outside-input"
        external.write_bytes(b"not an allowed input\n")
        paths = [self.share / "manifest", self.share / ".virtfs_metadata_root",
                 self.share / ".virtfs_metadata/nix", self.share / "bin/lua",
                 self.share / "nix/store/tiny-lua/lib/runtime.so",
                 self.share / "nix", self.share / "nix/store/tiny-lua/lib"]
        for index, path in enumerate(paths):
            with self.subTest(symlink=str(path.relative_to(self.share))):
                saved = self.root / "temporarily-removed-entry"
                path.rename(saved)
                path.symlink_to(self.root if saved.is_dir() else external)
                before = self.input_snapshot()
                output = self.root / f"host-symlink-{index}"
                result = self.prepare(output)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("symlink", result.stderr.lower())
                self.assertFalse((output / "wasmer.toml").exists())
                self.assertEqual(external.read_bytes(), b"not an allowed input\n")
                self.assertEqual(self.input_snapshot(), before)
                path.unlink()
                saved.rename(path)

    @unittest.skipUnless(hasattr(os, "mkfifo"), "requires POSIX FIFO support")
    def test_selected_special_file_is_rejected(self):
        fifo = self.share / "nix/store/tiny-lua/lib/host-fifo"
        os.mkfifo(fifo)
        self.write_entry_metadata(fifo, stat.S_IFIFO | 0o600)
        before = self.input_snapshot()
        result = self.prepare()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("special", result.stderr.lower())
        self.assertFalse((self.output / "wasmer.toml").exists())
        self.assertEqual(self.input_snapshot(), before)


if __name__ == "__main__":
    unittest.main()
