#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Include the real implementation, replacing only process/time primitives.
 * The synthetic PID is never passed to a kernel process operation. */
static int test_posix_spawn(pid_t *, const char *,
    const posix_spawn_file_actions_t *, const posix_spawnattr_t *,
    char *const [], char *const []);
static int test_kill(pid_t, int);
static pid_t test_waitpid(pid_t, int *, int);
static int test_clock_gettime(clockid_t, struct timespec *);
static int test_nanosleep(const struct timespec *, struct timespec *);
#define main exec_api_main_not_called
#define posix_spawn test_posix_spawn
#define kill test_kill
#define waitpid test_waitpid
#define clock_gettime test_clock_gettime
#define nanosleep test_nanosleep
#include "exec-api.c"
#undef main
#undef posix_spawn
#undef kill
#undef waitpid
#undef clock_gettime
#undef nanosleep

enum scenario { KILL_DENIED_WAIT_ZERO, KILL_OK_WAIT_ECHILD,
                WAIT_EINTR_THEN_REAP, SPAWN_FAILURE };
static enum scenario current;
static const pid_t synthetic_pid = 424242;
static int spawn_calls, kill_calls, wait_calls, sleep_calls, cancel_on_spawn;
static int64_t virtual_ns;
static char *fake_argv[] = { (char *)"never-executed-synthetic-guest", NULL };

static int test_posix_spawn(pid_t *pid, const char *path,
    const posix_spawn_file_actions_t *actions, const posix_spawnattr_t *attr,
    char *const argv[], char *const envp[])
{
    ++spawn_calls;
    assert(!strcmp(path, fake_argv[0]));
    assert(actions != NULL && attr == NULL);
    assert(argv == fake_argv && envp == environ);
    if (current == SPAWN_FAILURE) return EAGAIN;
    *pid = synthetic_pid;
    /* Cancel only after ownership is established, permitting a production
     * pre-spawn cancellation optimization without bypassing cleanup tests. */
    j.cancel = cancel_on_spawn;
    return 0;
}

static int test_kill(pid_t pid, int sig)
{
    ++kill_calls;
    assert(pid == synthetic_pid && sig == SIGKILL);
    assert(current != SPAWN_FAILURE);
    if (current == KILL_DENIED_WAIT_ZERO) { errno = EPERM; return -1; }
    return 0;
}

static pid_t test_waitpid(pid_t pid, int *status, int options)
{
    ++wait_calls;
    assert(pid == synthetic_pid && options == WNOHANG);
    assert(current != SPAWN_FAILURE);
    if (current == KILL_DENIED_WAIT_ZERO) return 0;
    if (current == KILL_OK_WAIT_ECHILD) { errno = ECHILD; return -1; }
    assert(current == WAIT_EINTR_THEN_REAP);
    if (wait_calls == 1) { errno = EINTR; return -1; }
    assert(wait_calls == 2);
    *status = SIGKILL;
    return pid;
}

static int test_clock_gettime(clockid_t clock_id, struct timespec *out)
{
    assert(clock_id == CLOCK_MONOTONIC);
    out->tv_sec = virtual_ns / 1000000000;
    out->tv_nsec = virtual_ns % 1000000000;
    return 0;
}

static int test_nanosleep(const struct timespec *requested, struct timespec *remaining)
{
    ++sleep_calls;
    assert(remaining == NULL);
    assert(requested->tv_sec == 0 && requested->tv_nsec == 50000000);
    virtual_ns += requested->tv_sec * INT64_C(1000000000) + requested->tv_nsec;
    return 0;
}

static void reset(enum scenario scenario, int cancel_after_spawn)
{
    memset(&j, 0, sizeof j);
    j.id = 41;
    j.state = JOB_BOOTING;
    j.cancel = 0;
    cancel_on_spawn = cancel_after_spawn;
    j.exit_code = -1;
    j.source_n = 8;
    memcpy(j.source, "original", 8);
    active = 1;
    shutdown_requested = public_poc = response_pending = 0;
    accepted_jobs = 1;
    next_admission = 0;
    current = scenario;
    spawn_calls = kill_calls = wait_calls = sleep_calls = 0;
    virtual_ns = 0;
    childargs = fake_argv;
}

/* Exercise actual HTTP parsing and admission logic. No worker is launched. */
static void post_run(int expected_status, int preserve_job)
{
    struct job before = j;
    unsigned before_accepted = accepted_jobs;
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    assert(fd_flags(pair[1], 1) == 0);
    const char request[] = "POST /run HTTP/1.1\r\nHost: local-test\r\n"
        "Content-Length: 10\r\nX-Source-Length: 10\r\n\r\nprint(123)";
    assert(write(pair[0], request, sizeof request - 1) == (ssize_t)(sizeof request - 1));
    assert(handle(pair[1]) == 0);
    assert(close(pair[1]) == 0);
    char response[2048];
    size_t used = 0;
    for (;;) {
        ssize_t got = read(pair[0], response + used, sizeof response - 1 - used);
        assert(got >= 0);
        if (!got) break;
        used += (size_t)got;
        assert(used < sizeof response - 1);
    }
    response[used] = 0;
    assert(close(pair[0]) == 0);
    char status_line[64];
    snprintf(status_line, sizeof status_line, "HTTP/1.1 %d Result\r\n", expected_status);
    assert(strncmp(response, status_line, strlen(status_line)) == 0);
    if (preserve_job) {
        assert(memcmp(&j, &before, sizeof j) == 0);
        assert(accepted_jobs == before_accepted);
        assert(strstr(response, "{\"error\":\"busy\"}") != NULL);
        assert(active == 1);
    } else {
        assert(j.id == before.id + 1 && j.state == JOB_BOOTING);
        assert(j.source_n == 10 && !memcmp(j.source, "print(123)", 10));
        assert(j.stdin_n == 0 && j.cancel == 0 && j.error[0] == 0);
        assert(active == 1 && accepted_jobs == before_accepted + 1);
    }
    assert(spawn_calls == 1); /* Admission did not run an unmocked worker. */
}

static void assert_quarantined(void)
{
    assert(j.state == JOB_FAILED && j.state == 6);
    assert(j.reaped == 0 && active == 1);
    assert(j.exit_code == -1 && j.output_n == 0);
    assert(!strcmp(j.error, "child reap failed; instance quarantined"));
    assert(spawn_calls == 1 && kill_calls == 1);
    post_run(429, 1);
}

int main(void)
{
    /* Valid descriptor needed by actual spawn action construction; no listener. */
    listener = open("/dev/null", O_RDONLY | O_CLOEXEC);
    assert(listener > STDERR_FILENO);

    reset(KILL_DENIED_WAIT_ZERO, 1);
    run_job();
    assert_quarantined();
    assert(virtual_ns == INT64_C(10000000000));
    assert(wait_calls == 200 && sleep_calls == 200);
    puts("PASS kill=EPERM + waitpid=0: virtual 10s bound, state=6/reaped=false/active=1; POST /run=429, job unchanged");

    reset(KILL_OK_WAIT_ECHILD, 1);
    run_job();
    assert_quarantined();
    assert(wait_calls == 1 && sleep_calls == 0 && virtual_ns == 0);
    puts("PASS kill=0 + waitpid=ECHILD: state=6/reaped=false/active=1; POST /run=429, job unchanged");

    reset(WAIT_EINTR_THEN_REAP, 1);
    run_job();
    assert(j.state == JOB_CANCELLED && j.state == 4);
    assert(j.reaped == 1 && active == 0 && j.error[0] == 0);
    assert(spawn_calls == 1 && kill_calls == 1 && wait_calls == 2 && sleep_calls == 0);
    post_run(202, 0);
    puts("PASS waitpid=EINTR then PID: state=4/reaped=true/active=0; next POST /run=202");

    reset(SPAWN_FAILURE, 0);
    run_job();
    assert(j.state == JOB_FAILED && j.reaped == 1 && active == 0);
    assert(!strcmp(j.error, "spawn failed"));
    assert(spawn_calls == 1 && kill_calls == 0 && wait_calls == 0 && sleep_calls == 0);
    post_run(202, 0);
    puts("PASS spawn=EAGAIN: state=6/reaped=true/active=0; no kill/wait; next POST /run=202");

    reset(WAIT_EINTR_THEN_REAP, 0);
    j.cancel = 1;
    run_job();
    assert(j.state == JOB_CANCELLED && j.reaped == 1 && active == 0);
    assert(j.error[0] == 0 && j.exit_code == -1);
    assert(spawn_calls == 0 && kill_calls == 0 && wait_calls == 0 && sleep_calls == 0);
    puts("PASS cancellation before run_job: state=4/reaped=true/active=0; zero spawn/kill/wait");

    public_poc = 1;
    public_expires_at = 0;
    accepted_jobs = PUBLIC_JOB_LIMIT;
    admission_window_start = 0;
    next_admission = 0;
    virtual_ns = INT64_C(599000000000);
    assert(!public_expired() && public_admission_limited());
    virtual_ns = INT64_C(600000000000);
    assert(!public_admission_limited() && accepted_jobs == 0);
    next_admission = 610;
    assert(public_admission_limited());
    virtual_ns = INT64_C(610000000000);
    assert(!public_admission_limited());
    puts("PASS persistent admission: exhausted window rejects, 600s resets, spacing retained; no expiry configured");
    assert(close(listener) == 0);
    puts("ALL 6 CONTROL-FLOW SCENARIOS PASSED. No guest, child process, listener, Wasm or deployment launched.");
    return 0;
}
