"""Fake-child synchronous public mode tests; not Linux/Edge evidence.

The fake child writes one reserved marker in its mapped build directory. Do not
run concurrent instances of this test against the same directory or TCP port.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import time
import urllib.error
import urllib.request


parser = argparse.ArgumentParser()
for name in ("wasm", "fake", "out"):
    parser.add_argument(name, type=Path)
a = parser.parse_args()
out = a.out.resolve()
out.mkdir()
build = a.fake.resolve().parent
busy_marker = build / "fake-busy-entered"
busy_marker.unlink(missing_ok=True)
log_path = out / "sync-test.log"
report = {"result": "failed"}
process = None
busy_socket = None
log = log_path.open("wb")


def request(path, method="GET", body=None, headers=None):
    req = urllib.request.Request(
        "http://127.0.0.1:18082" + path,
        method=method, data=body, headers=headers or {},
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        with error:
            return error.code, json.load(error)


def submit_pass():
    return request("/execute", "POST", b"PASS")


def assert_exact_success(code, result):
    assert code == 200, (code, result)
    assert result["state"] == 3, result
    assert result["exit_code"] == 7, result
    assert result["reaped"] is True, result
    assert result["error"] == "", result
    assert base64.b64decode(result["output_base64"], validate=True) == b"fake\0bytes", result


def read_log():
    return log_path.read_text(errors="replace")


def wait_for(check, description, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f"supervisor exited before {description}: {read_log()}")
        value = check()
        if value:
            return value
        time.sleep(0.05)
    raise AssertionError(f"deadline waiting for {description}: {read_log()}")


def wait_until(deadline):
    time.sleep(max(0, deadline - time.monotonic()))


try:
    process = subprocess.Popen(
        ["wasmer", "run", "--offline", "--net", "--no-tty",
         "--env", "PUBLIC_POC=1", "--env", f"POC_EXPIRES_AT={int(time.time())+300}", "--env", "PORT=18082",
         "--volume", f"{build}:/probe", str(a.wasm.resolve()),
         "--", "/probe/" + a.fake.name],
        stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
    )

    def healthy():
        try:
            return request("/health")[0] == 200
        except urllib.error.URLError:
            return False

    wait_for(
        lambda: "Lua API ready port=18082 mode=public-poc" in read_log(),
        "this supervisor's listener-ready log",
    )
    wait_for(healthy, "health endpoint")
    assert request("/jobs/1")[0] == 404
    assert request("/jobs/1/cancel", "POST")[0] == 404
    assert request("/run", "POST")[0] == 404

    first_started = time.monotonic()
    assert_exact_success(*submit_pass())
    report["synchronous_output"] = True
    # Give the responder time to release ownership before exercising the rate
    # gate. The later BUSY check separately tests serialization after its gate.
    time.sleep(0.1)
    assert submit_pass()[0] == 429
    wait_until(first_started + 10.2)

    busy_socket = socket.create_connection(("127.0.0.1", 18082), timeout=5)
    busy_socket.sendall(
        b"POST /execute HTTP/1.1\r\nContent-Length: 4\r\n"
        b"X-Source-Length: 4\r\n\r\nBUSY"
    )
    wait_for(
        lambda: busy_marker.exists() and busy_marker.read_bytes() == b"busy-entered\n",
        "fake child BUSY entry marker",
    )
    entered = time.monotonic()
    child_match = wait_for(
        lambda: re.search(r"(?m)^job 2 child pid=(\d+)$", read_log()),
        "BUSY job 2 spawn log",
    )
    report["busy_child_pid"] = int(child_match.group(1))
    report["busy_child_entered"] = True
    health_started = time.monotonic()
    assert request("/health")[0] == 200
    report["health_during_execution_seconds"] = time.monotonic() - health_started

    # At this point the second job's ten-second admission interval is over.
    # A 429 must reflect the running job, not merely that rate interval.
    wait_until(entered + 10.1)
    assert not re.search(r"(?m)^job 2 reaped=", read_log()), read_log()
    assert submit_pass()[0] == 429
    report["busy_after_rate_interval"] = True

    disconnected_at = time.monotonic()
    # Combined with the 12-second reap wait, this finishes before the fake
    # child's 30-second execution deadline. Timeout cannot masquerade as cancel.
    assert disconnected_at - entered < 15, "BUSY check consumed cancellation timing margin"
    busy_socket.close()
    busy_socket = None
    reaped = wait_for(
        lambda: re.search(r"(?m)^job 2 reaped=1 status=(\d+)$", read_log()),
        "BUSY job 2 confirmed reap after disconnect", seconds=12,
    )
    report["disconnect_reap_seconds"] = time.monotonic() - disconnected_at
    report["disconnect_reap_status"] = int(reaped.group(1))

    # The reap log is emitted just before terminal publication. Allow only
    # transient busy responses while that final publication completes.
    fresh_result = None

    def fresh_response():
        global fresh_result
        code, result = submit_pass()
        if code == 429:
            return False
        assert_exact_success(code, result)
        fresh_result = result
        return True

    wait_for(fresh_response, "exact fresh response after confirmed reap", seconds=3)
    report["disconnect_cancel_then_fresh"] = True
    report["fresh_response"] = fresh_result

    time.sleep(10.1)
    assert submit_pass()[0] == 429
    report.update(result="passed", public_jobs_routes_hidden=True,
                  rate_and_total_cap=True)
except Exception as error:
    report["error"] = str(error)
    raise
finally:
    if busy_socket is not None:
        busy_socket.close()
    if process is not None:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait()
    log.close()
    (out / "sync-test-result.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
