# Local arbitrary-Lua API prototype

Not an Edge deployment or a public execution service. The development listener is fixed to loopback port18081; there is no authentication, ownership enforcement, CORS allowance or abuse protection. Do not expose it through a tunnel/proxy. Job IDs are counters, not access credentials. The server exits after ten minutes.

## Execution contract

- POST /run accepts an application/octet-stream body: Lua source bytes followed immediately by stdin bytes. Content-Length is the combined byte count; X-Source-Length is the source byte count. Each portion is limited to64KiB. Chunked transfer and duplicate length fields are rejected.
- Returns202 with a job ID immediately. One job may own a guest at a time; additional submissions receive429 rather than an unbounded queue.
- GET /jobs/{id} reports state, exit_code, reaped, error, and output_base64. Output is combined stdout/stderr, binary-safe, returned after execution, not streamed. Only the current/most recent result is retained.
- POST /jobs/{id}/cancel requests cancellation. A terminal response with reaped=true means the WASIX child has actually been waited for, not just signalled.
- States:0idle,1booting,2executing,3finished (inspect exit_code),4cancelled,5timeout,6supervisor failure. A Lua error is a finished execution with a nonzero exit code.
- Boot budget180s, execution budget30s, bounded HTTP reads/writes and 128KiB raw-output cap with a separate256KiB encoded transport buffer. Fresh guest per accepted request, including after cancellation. No warm guest reuse across callers.

## Guest boundary

The package export is guest-read-only. Lua runs as guest UID/GID65534 with a fresh writable /work, not as the guest root shell. It may use Lua os.execute and ordinary guest programs. The guest has no network device. Source/stdin are base64-transferred, not interpolated as executable shell syntax. A noninteractive root shell reads the generated command without BusyBox interactive line-editor truncation, creates the input/output files, captures user output, and emits base64 output plus exit status on its control console afterward. Lua is detached from that controlling-terminal session and console device permissions are restricted. User output that resembles a supervisor marker remains data.

This protects the protocol against tested straightforward forgery; it does not establish resistance to guest-kernel/QEMU/Wasmer exploits. The host runtime still grants networking to the supervisor and its child environment, even though the guest has no NIC. Host-mounted directories are dedicated build assets, not user secrets or host root. An actual deployment needs runtime-enforced least privilege, authentication/job ownership, rate/concurrency/spending controls, quota verification and adversarial review.

## Local evidence

The earlier fixed-workload HTTP smoke test proved listening before guest boot, WASIX subprocess/pipes, two Lua executions and cancellation. The first arbitrary-source three-guest test then passed:
- Source+stdin handling, UID and supplementary group65534, denied /dev/tty, /dev/console and /dev/ttyS0 access.
- Marker-looking output remained ordinary output.
- Fresh request lacked the prior /work canary and returned genuine Lua error/exit1.
- Infinite-loop request cancelled with successful kill/wait and confirmed child reaping.
- Final hardened replay: total76.791s; health response0.108s; peak child RSS779,088KiB. This run used an existing host compilation cache.

Separate fake-guest tests passed malformed header/body rejection, fragmented requests, split exit-status records, binary NUL output, exact cancellation routes, busy rejection and cancellation followed by a new request. These are protocol tests, not Linux or hostile-isolation tests. Final hardened source passed the real-guest matrix and fake-guest regressions. Maximum-size source/stdin, output-boundary and forced-cleanup-failure tests remain additional gates.

## Build and local tests

With the pinned WASIX toolchain on PATH:

```sh
bash ports/wasix/edge/build.sh ./edge-build
python3 ports/wasix/edge/test-protocol.py ./edge-build/exec-api.wasm ./edge-build/fake-guest.wasm ./protocol-results
python3 ports/wasix/edge/test-http.py ./edge-build/exec-api.wasm ./qemu-build/qemu-system-x86_64.wasm ./guest-pack ./mapped-lua-share ./http-results
# Additional boundary gate, sequential fresh guests:
python3 ports/wasix/edge/test-boundaries.py ./edge-build/exec-api.wasm ./qemu-build/qemu-system-x86_64.wasm ./guest-pack ./mapped-lua-share ./boundary-results
```

Output directories must not exist. Use disposable dedicated filesystem mounts and external memory/time limits. `--net` is needed for local HTTP but is not a deny-by-default networking policy. Port18081 must be free. Tests deliberately kill their owned host process group after completion. No user source is sent outside this local Wasmer process.
