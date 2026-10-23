#!/usr/bin/env python3
"""Local ABI gates, not a Linux boot or Wasmer Edge compatibility test."""
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time

SOURCE = Path(__file__).with_name('capabilities.c')

def run(args, timeout=30):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, start_new_session=True)
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        out, err = proc.communicate()
        return {'status': 'killed_at_deadline', 'returncode': proc.returncode,
                'stdout': out, 'stderr': err}
    return {'status': 'exited', 'returncode': proc.returncode,
            'stdout': out, 'stderr': err}

results = {'scope': 'local WASIX boundary probes; no QEMU boot or Edge deployment',
           'wasmer': run(['wasmer', '--version']),
           'compiler': run(['wasixcc', '--version']), 'tests': {}}
with tempfile.TemporaryDirectory(prefix='trynix-wasix-') as td:
    wasm = str(Path(td) / 'capabilities.wasm')
    compile_args = ['wasixcc', '-O2', '-pthread', '-Wl,--initial-memory=805306368',
                    '-Wl,--max-memory=2147483648', str(SOURCE), '-o', wasm]
    subprocess.run(compile_args, check=True)
    results['artifact_bytes'] = Path(wasm).stat().st_size
    for name in ('fresh-instance-1', 'fresh-instance-2'):
        r = run(['wasmer', 'run', wasm])
        results['tests'][name] = r
        assert r['returncode'] == 0 and 'context: PASS' in r['stdout'], r
    start = time.monotonic()
    r = run(['wasmer', 'run', wasm, '--', 'loop'], timeout=2)
    r['elapsed_seconds'] = round(time.monotonic() - start, 3)
    results['tests']['cancel'] = r
    assert r['status'] == 'killed_at_deadline' and 'loop: READY' in r['stdout'], r
    r = run(['wasmer', 'run', wasm])
    results['tests']['fresh-after-cancel'] = r
    assert r['returncode'] == 0 and 'fresh-instance: PASS' in r['stdout'], r
print(json.dumps(results, indent=2))
