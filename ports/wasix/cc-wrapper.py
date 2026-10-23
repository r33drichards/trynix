#!/usr/bin/env python3
"""Adapt GNU archive-group flags emitted by Meson to wasm-ld.

wasm-ld resolves archive symbols without GNU ld's explicit group markers.
Do not remove arbitrary unknown flags: unsupported inputs must remain errors.
"""
from pathlib import Path
import os
import sys

compiler, *args = sys.argv[1:]
args = [arg for arg in args if arg not in ('-Wl,--start-group', '-Wl,--end-group')]
# QEMU names static archives .fa; wasixcc 0.4.7 classifies them as C inputs.
# An adjacent .a alias preserves relative paths inside LLVM thin archives.
for index, arg in enumerate(args):
    if arg.endswith('.fa') and Path(arg).is_file():
        source = Path(arg)
        alias = source.with_name(source.name + '.a')
        if alias.exists() or alias.is_symlink():
            if alias.resolve() != source.resolve():
                raise RuntimeError(f"Archive alias collision: {alias}")
        else:
            alias.symlink_to(source.name)
        args[index] = str(alias)
os.execvp(compiler, [compiler, *args])
