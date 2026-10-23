#!/usr/bin/env python3
"""Supervisor regression tests with a fake child, not QEMU/Linux evidence."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

RUNNER = Path(__file__).with_name('run-guest.py')

class RunnerTests(unittest.TestCase):
    def test_setup_and_deadline(self):
        with tempfile.TemporaryDirectory(prefix='wasix-runner-') as td:
            root = Path(td)
            (root/'machine.json').write_text('{"args": []}')
            wasm = root/'fake.wasm'
            wasm.write_bytes(b'fake')
            marker = root/'started'
            fake = root/'wasmer'
            fake.write_text('#!' + sys.executable + '\n'
                'from pathlib import Path\nimport time\n'
                f'Path({str(marker)!r}).write_text("started")\n'
                'print("fake child ready", flush=True)\ntime.sleep(30)\n')
            fake.chmod(0o755)
            env = dict(os.environ, PATH=str(root)+os.pathsep+os.environ['PATH'])
            cmd = [sys.executable, str(RUNNER), str(wasm), td, td]
            bad = subprocess.run(cmd+['--log', str(root/'missing'/'log')], env=env,
                                 capture_output=True, timeout=5)
            self.assertNotEqual(bad.returncode, 0)
            self.assertFalse(marker.exists(), 'bad log must not spawn child')
            run = subprocess.run(cmd+['--log', str(root/'log'), '--timeout', '0.5'],
                                 env=env, capture_output=True, text=True, timeout=5)
            self.assertEqual(run.returncode, 1)
            result = json.loads(run.stdout)
            self.assertEqual(result['result'], 'deadline')
            self.assertEqual(result['returncode'], -9)
            self.assertFalse(result['linux_handshake'])
            self.assertTrue(marker.exists())
            self.assertIn('fake child ready', (root/'log').read_text())
            invalid = subprocess.run(cmd+['--timeout', 'nan'], env=env,
                                     capture_output=True, timeout=5)
            self.assertEqual(invalid.returncode, 2)

if __name__ == '__main__':
    unittest.main()
