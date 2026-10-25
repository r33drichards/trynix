# Local arbitrary-Lua API prototype

Not an Edge deployment or a public execution service. By default the development listener is loopback port18081; there is no authentication, ownership enforcement, CORS allowance or abuse protection. Do not expose the default development routes through a tunnel/proxy. Its job IDs are counters, not access credentials. An opt-in synchronous POC mode is described below and still requires deployment gates. The server exits after ten minutes.

## Execution contract

- POST /run accepts an application/octet-stream body: Lua source bytes followed immediately by stdin bytes. Content-Length is the combined byte count; X-Source-Length is the source byte count. Each portion is limited to8KiB. Chunked transfer and duplicate length fields are rejected.
- Returns202 with a job ID immediately. One job may own a guest at a time; additional submissions receive429 rather than an unbounded queue.
- GET /jobs/{id} reports state, exit_code, reaped, error, and output_base64. Output is combined stdout/stderr, binary-safe, returned after execution, not streamed. Only the current/most recent result is retained.
- POST /jobs/{id}/cancel requests cancellation. A terminal response with reaped=true means the WASIX child has actually been waited for, not just signalled.
- States:0idle,1booting,2executing,3finished (inspect exit_code),4cancelled,5timeout,6supervisor failure. A Lua error is a finished execution with a nonzero exit code.
- Boot budget180s, input-staging budget60s, execution budget30s after a trusted start marker, bounded HTTP reads/writes and 128KiB raw-output cap with a separate256KiB encoded transport buffer. Fresh guest per accepted request, including after cancellation. No warm guest reuse across callers.

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

Separate fake-guest tests passed malformed header/body rejection, fragmented requests, split exit-status records, binary NUL output, exact cancellation routes, busy rejection and cancellation followed by a new request. These are protocol tests, not Linux or hostile-isolation tests. Final hardened source passed the real-guest matrix and fake-guest regressions. A later strict matrix passed full8KiB source and8KiB stdin, exact128KiB output, over-limit output capped at128KiB with exit153, and a30.17s execution timeout, with all four children reaped. The earlier64KiB+64KiB serial-input attempt timed out and is not supported by the POC. Native injected kill/reap-failure quarantine tests also passed; these establish control flow, not runtime kill guarantees.

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


## Opt-in synchronous public POC path (locally validated, not deployed)

Public mode is persistent by default: no scheduled expiry or supervisor lifetime shutdown. Optional POC_EXPIRES_AT is supported only when explicitly configured; this deployment does not configure it. PUBLIC_POC=1 changes the listener to all interfaces (PORT, default80) and exposes only POST /execute and GET /health. /run and /jobs are unavailable, so results and cancellation do not depend on cross-instance affinity and no public job identifier grants access to another caller's result. /execute accepts plain source as the whole body when X-Source-Length is omitted, or uses the same source/stdin body framing when supplied and returns one JSON response with state, exit_code, reaped, error and output_base64. Output is still buffered, not live-streamed. HTTP200 includes completed Lua errors with nonzero exit_code; timeout returns504 and supervisor failure502.

One response thread owns the accepted execution connection while the main thread continues processing bounded health/busy requests. The job cannot be replaced until its response has been delivered or discarded. Closing the client connection, including a write-half-close, requests cancellation; clients must keep both directions open while awaiting the response. Edge proxy propagation of disconnects is not verified, so fixed execution and reap deadlines remain the enforcement boundary. Slow clients can delay the serial request-intake loop by up to the bounded read/write deadlines; this is not a production DoS-resistant server.

The public mode admits at most three jobs per fixed ten-minute instance window, with at least ten seconds between admissions and no queue. These counters reset on new instances and are NOT a global spending cap. The user approved persistent unauthenticated testing within the shared Free quota, including the risk to other apps. No paid upgrade is authorized. No deployment manifest here should be read as evidence that such a cap exists. The server remains listening between admission windows. No InstaBoot, volumes, secrets or registry tokens are configured.


The final local package-container smoke passed HTTP200, actual Linux/Lua source+stdin, binary-safe combined output, exit0 and reaping in26.629s, peak793228KiB. It used the packaged filesystem without external volume mounts and an existing host compilation cache. See evidence/public-package.json and public-artifacts.json. Public transport tests independently prove BUSY admission before disconnect, actual reaping within0.101s, exact next-request output, hidden job routes and per-process rate/count limits. Absolute expiry controls and five native quarantine scenarios also passed. No Edge quota, gateway timeout, backend or hostile escape resistance is inferred from these results.

For local container preparation, use prepare-package.py with the supervisor, QEMU, guest pack, mapped share and a new output directory; then wasmer package build on that directory. The generated manifest has no expiry and defaults to port80 (override PORT for local tests). It includes no registry credentials or deployment action. Check all binary redistribution license/corresponding-source requirements before registry publication; this PR publishes source/test evidence only.

Persistent-mode revision: no-expiry startup, optional-expiry validation, public fake-child protocol and six injected native control-flow scenarios pass. The sixth scenario advances a fake clock across the ten-minute admission-window reset; it is not a 10-minute wall-clock service test. Previous actual Linux package evidence applies to the earlier supervisor hash.
