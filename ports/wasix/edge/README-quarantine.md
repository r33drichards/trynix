# Native quarantine control-flow regression test

Run `./test-quarantine.sh OUTPUT_DIRECTORY` from this directory, or invoke the script by path. The output directory is required and is created if missing. The binary, run log, and compiler temporary artifacts are written there, not beside production source. A POSIX shell, C compiler, pthreads, and `sha256sum` are required; `CC` can select the compiler executable. The runner records the production source hash before and after compilation/execution and fails if it changes.

The test includes the actual `exec-api.c`, renames its main function, and replaces only `posix_spawn`, `kill`, `waitpid`, `clock_gettime`, and `nanosleep` with deterministic test-TU functions. The synthetic PID is never sent to a kernel process operation. Cleanup tests set cancellation inside successful fake spawn, after PID assignment, so they still exercise cleanup when a production pre-spawn cancellation check is present.

Assertions cover:

1. `kill=EPERM`, `waitpid=0`: exactly 200 virtual 50 ms sleeps reach 10 seconds; state 6, reaped false, active true, and quarantine error. Real `POST /run` parsing/admission returns 429 without altering the job or accepted-job counter.
2. `kill=0`, `waitpid=ECHILD`: immediate quarantine with the same state and unchanged-job 429. ECHILD is not accepted as proof of reaping.
3. `waitpid=EINTR` then the owned PID: cancelled state 4, reaped true, active false, no error; next request returns 202.
4. `posix_spawn=EAGAIN` without cancellation: failed state 6, reaped true, active false, spawn-failed error, no kill/wait; next request returns 202.
5. Cancellation before `run_job`: cancelled state 4, reaped true, active false, no error, zero spawn/kill/wait/sleep calls.

The real request handler is exercised using local Unix socketpairs. No worker thread is launched, so successful admission checks stop before execution. This is native C control-flow evidence only, not proof of Wasmer/WASIX process semantics, actual kill/reap behavior, guest execution, HTTP concurrency, public synchronous responses, isolation, or deployment. The test does not launch a guest or child process, bind a network listener, build Wasm, or deploy anything.
