#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define INPUT_MAX 8192
#define OUTPUT_MAX 131072
#define HEADER_MAX 8192
#define REQUEST_MAX (2 * INPUT_MAX + HEADER_MAX)
/* Includes base64 expansion, CRLF wrapping, framing and boot diagnostics. */
#define TRANSPORT_MAX 262144
#define BOOT_SECONDS 180
#define EXEC_SECONDS 30
#define STAGE_SECONDS 60
#define IO_SECONDS 10
#define REAP_SECONDS 10

enum job_state {
    JOB_EMPTY = 0,
    JOB_BOOTING = 1,
    JOB_RUNNING = 2,
    JOB_COMPLETE = 3, /* The Lua exit code may be nonzero. */
    JOB_CANCELLED = 4,
    JOB_TIMEOUT = 5,
    JOB_FAILED = 6
};

/* Loopback development API; opt-in public mode exposes synchronous /execute only.
 * Only one worker owns a job. A terminal state is published after cleanup;
 * unconfirmed reaping permanently quarantines this instance from new jobs.
 */
struct job {
    unsigned id;
    int state, cancel, exit_code, reaped;
    size_t source_n, stdin_n, output_n;
    unsigned char source[INPUT_MAX], input[INPUT_MAX], output[OUTPUT_MAX];
    char error[128];
};

static struct job j;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static char **childargs;
static int listener = -1;
static int shutdown_requested, active;
static int public_poc, response_pending;
static unsigned accepted_jobs;
static double next_admission;
static uint64_t public_expires_at;
#define PUBLIC_JOB_LIMIT 3
#define PUBLIC_ADMISSION_SECONDS 10

static double now(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) {
        /* Deadlines are a requirement, not an optional best effort. */
        perror("clock_gettime");
        abort();
    }
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* Wall-clock expiry is shared configuration across all fresh instances.
 * Missing/broken clock fails closed. This is exposure duration, not a cost cap.
 */
static int public_expired(void)
{
    if (!public_poc) return 0;
    struct timespec t;
    return clock_gettime(CLOCK_REALTIME, &t) || t.tv_sec < 0 ||
        (uint64_t)t.tv_sec >= public_expires_at;
}

static int cancelled(void)
{
    pthread_mutex_lock(&lock);
    int result = j.cancel || shutdown_requested || public_expired();
    pthread_mutex_unlock(&lock);
    return result;
}

static int fd_flags(int fd, int nonblocking)
{
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
        return -1;
    }
    if (nonblocking) {
        flags = fcntl(fd, F_GETFL);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }
    return 0;
}

static int write_until(int fd, const void *data, size_t n,
                       double deadline, int guest_pipe)
{
    const char *s = data;
    while (n) {
        if (now() >= deadline || (guest_pipe && cancelled())) {
            return -1;
        }
        ssize_t written = write(fd, s, n);
        if (written > 0) {
            s += written;
            n -= (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd p = { .fd = fd, .events = POLLOUT };
            int r = poll(&p, 1, 50);
            if (r < 0 && errno != EINTR) {
                return -1;
            }
            if (r > 0 && (p.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                return -1;
            }
            continue;
        }
        return -1;
    }
    return 0;
}

static int send_guest(int fd, const char *s)
{
    return write_until(fd, s, strlen(s), now() + IO_SECONDS, 1);
}

static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *b64(const unsigned char *s, size_t n)
{
    char *out = malloc((n + 2) / 3 * 4 + 1);
    if (!out) {
        return NULL;
    }
    size_t k = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)s[i] << 16;
        if (i + 1 < n) v |= (unsigned)s[i + 1] << 8;
        if (i + 2 < n) v |= s[i + 2];
        out[k++] = alphabet[v >> 18];
        out[k++] = alphabet[(v >> 12) & 63];
        out[k++] = i + 1 < n ? alphabet[(v >> 6) & 63] : '=';
        out[k++] = i + 2 < n ? alphabet[v & 63] : '=';
    }
    out[k] = 0;
    return out;
}

static int unb64(const char *s, size_t n, unsigned char *out, size_t *count)
{
    size_t k = 0;
    unsigned q[4], qn = 0;
    int ended = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            continue;
        }
        if (ended) return -1;
        if (c == '=') {
            q[qn++] = 64;
        } else {
            const char *p = c ? strchr(alphabet, c) : NULL;
            if (!p) return -1;
            q[qn++] = (unsigned)(p - alphabet);
        }
        if (qn != 4) continue;
        if (q[0] >= 64 || q[1] >= 64 || (q[2] == 64 && q[3] != 64)) {
            return -1;
        }
        size_t bytes = q[2] == 64 ? 1 : (q[3] == 64 ? 2 : 3);
        if (bytes > OUTPUT_MAX - k ||
            (bytes == 1 && (q[1] & 15)) ||
            (bytes == 2 && (q[2] & 3))) {
            return -1;
        }
        out[k++] = (unsigned char)((q[0] << 2) | (q[1] >> 4));
        if (bytes >= 2) out[k++] = (unsigned char)((q[1] << 4) | (q[2] >> 2));
        if (bytes == 3) out[k++] = (unsigned char)((q[2] << 6) | q[3]);
        ended = bytes != 3;
        qn = 0;
    }
    if (qn) return -1;
    *count = k;
    return 0;
}

/* Kill and wait are bounded. ECHILD is not proof that we reaped our child. */
static int terminate_and_reap(pid_t pid, int *status)
{
    int killed = kill(pid, SIGKILL);
    int kill_error = killed < 0 ? errno : 0;
    if (killed < 0 && kill_error != ESRCH) {
        fprintf(stderr, "kill pid=%d failed: %s\n", pid, strerror(kill_error));
    }
    double deadline = now() + REAP_SECONDS;
    do {
        pid_t result = waitpid(pid, status, WNOHANG);
        if (result == pid) return 0;
        if (result < 0 && errno != EINTR) return -1;
        if (result == 0) {
            struct timespec pause = { .tv_nsec = 50000000 };
            nanosleep(&pause, NULL);
        }
    } while (now() < deadline);
    errno = ETIMEDOUT;
    return -1;
}

static int complete_line(const char *buf, const char *line)
{
    size_t len = strlen(line);
    const char *p = buf;
    while ((p = strstr(p, line))) {
        if ((p == buf || p[-1] == '\n') &&
            (p[len] == '\n' || (p[len] == '\r' && p[len + 1] == '\n'))) {
            return 1;
        }
        ++p;
    }
    return 0;
}

/* Returns 1 only for a complete, validated result; 0 means more input. */
static int parse_result(char *buf, unsigned char *output, size_t *output_n,
                        int *exit_code, const char **error)
{
    char *begin = strstr(buf, "\nEXEC_BEGIN_MARK\r\n");
    if (!begin) begin = strstr(buf, "\nEXEC_BEGIN_MARK\n");
    if (!begin) return 0;
    begin = strchr(begin + 1, '\n') + 1;
    char *end = strstr(begin, "\nEXEC_END_MARK:");
    if (!end) return 0;
    char *line_end = strchr(end + 1, '\n');
    if (!line_end) return 0;
    const char *number = end + strlen("\nEXEC_END_MARK:");
    const char *number_end = line_end;
    if (number_end > number && number_end[-1] == '\r') --number_end;
    unsigned code = 0;
    if (number == number_end) {
        *error = "invalid exit record";
        return -1;
    }
    for (const char *p = number; p < number_end; ++p) {
        if (*p < '0' || *p > '9' || code > (255U - (unsigned)(*p - '0')) / 10) {
            *error = "invalid exit record";
            return -1;
        }
        code = code * 10 + (unsigned)(*p - '0');
    }
    if (unb64(begin, (size_t)(end - begin), output, output_n)) {
        *error = "invalid or excessive output encoding";
        return -1;
    }
    *exit_code = (int)code;
    return 1;
}

static char *guest_command(void)
{
    char *source = b64(j.source, j.source_n);
    char *input = b64(j.input, j.stdin_n);
    if (!source || !input) {
        free(source);
        free(input);
        return NULL;
    }
    /* Request bytes enter the shell only as generated base64 alphabet.
     * Job control is disabled by the acknowledged setup command before this
     * runs, so setsid does not need its process-group-leader fork fallback.
     * The pinned BusyBox ulimit -f uses 512-byte blocks: 256 = 128 KiB.
     * Lua has no controlling tty and cannot open the root-only serial devices.
     */
    const char *format =
        "if printf '%%s' '%s' | base64 -d > /tmp/request.lua && "
        "printf '%%s' '%s' | base64 -d > /tmp/request.stdin && "
        "printf 'nobody:x:65534:65534:nobody:/tmp:/bin/sh\\n' > /etc/passwd && "
        "chmod 644 /tmp/request.lua /tmp/request.stdin && "
        "mkdir -p /work && chown 65534:65534 /work && chmod 700 /work && "
        "chmod 600 /dev/tty /dev/console /dev/ttyS0 && umask 077 && "
        ": > /tmp/request.output && chmod 600 /tmp/request.output; then "
        "printf 'EXEC_RUNNING_%%s\\n' 'MARK'; "
        "(ulimit -f 256 || exit 125; "
        "exec setsid su -s /bin/sh nobody -c 'cd /work && exec lua /tmp/request.lua') "
        "< /tmp/request.stdin > /tmp/request.output 2>&1; code=$?; "
        "printf '\\nEXEC_BEGIN_%%s\\n' 'MARK'; "
        "if base64 /tmp/request.output; then "
        "printf '\\nEXEC_END_%%s:%%d\\n' 'MARK' \"$code\"; "
        "else printf 'SETUP_ERROR_%%s\\n' 'MARK'; fi; "
        "else printf 'SETUP_ERROR_%%s\\n' 'MARK'; fi\n";
    int length = snprintf(NULL, 0, format, source, input);
    char *command = length < 0 ? NULL : malloc((size_t)length + 1);
    if (command && snprintf(command, (size_t)length + 1, format, source, input) != length) {
        free(command);
        command = NULL;
    }
    free(source);
    free(input);
    return command;
}

static void run_job(void)
{
    int in[2] = {-1, -1}, out[2] = {-1, -1};
    pid_t pid = -1;
    int status = 0, child_reaped = 1;
    int ready = 0, prepared = 0, submitted = 0, executing = 0, done = 0;
    int terminal = JOB_FAILED, exit_code = -1;
    const char *error = NULL;
    char *buf = calloc(1, TRANSPORT_MAX + 1);
    unsigned char *output = malloc(OUTPUT_MAX);
    size_t used = 0, output_n = 0;
    double deadline = now() + BOOT_SECONDS;
    posix_spawn_file_actions_t actions;
    int actions_initialized = 0, rc;

    if (cancelled()) {
        terminal = JOB_CANCELLED;
        goto cleanup;
    }
    if (!buf || !output || pipe(in) || pipe(out)) {
        error = "pipe/allocation failed";
        goto cleanup;
    }
    for (int n = 0; n < 2; ++n) {
        /* Keep action close operations distinct from stdio destinations. */
        if (in[n] <= STDERR_FILENO || out[n] <= STDERR_FILENO ||
            fd_flags(in[n], n == 1) || fd_flags(out[n], n == 0)) {
            error = "pipe descriptor setup failed";
            goto cleanup;
        }
    }
    rc = posix_spawn_file_actions_init(&actions);
    if (rc) {
        error = "spawn actions initialization failed";
        goto cleanup;
    }
    actions_initialized = 1;
#define ADD_ACTION(call) do { if ((rc = (call)) != 0) { \
    error = "spawn actions setup failed"; goto cleanup; } } while (0)
    ADD_ACTION(posix_spawn_file_actions_adddup2(&actions, in[0], STDIN_FILENO));
    ADD_ACTION(posix_spawn_file_actions_adddup2(&actions, out[1], STDOUT_FILENO));
    ADD_ACTION(posix_spawn_file_actions_adddup2(&actions, out[1], STDERR_FILENO));
    for (int n = 0; n < 2; ++n) {
        ADD_ACTION(posix_spawn_file_actions_addclose(&actions, in[n]));
        ADD_ACTION(posix_spawn_file_actions_addclose(&actions, out[n]));
    }
    ADD_ACTION(posix_spawn_file_actions_addclose(&actions, listener));
#undef ADD_ACTION
    rc = posix_spawn(&pid, childargs[0], &actions, NULL, childargs, environ);
    if (rc) {
        pid = -1; /* A failed spawn does not establish ownership of a PID. */
        error = "spawn failed";
        goto cleanup;
    }
    child_reaped = 0;
    rc = posix_spawn_file_actions_destroy(&actions);
    actions_initialized = 0;
    if (rc) {
        error = "spawn actions destruction failed";
        goto cleanup;
    }
    close(in[0]);
    in[0] = -1;
    close(out[1]);
    out[1] = -1;
    fprintf(stderr, "job %u child pid=%d\n", j.id, pid);

    for (;;) {
        if (cancelled()) {
            terminal = JOB_CANCELLED;
            break;
        }
        if (now() >= deadline) {
            terminal = JOB_TIMEOUT;
            error = executing ? "execution timeout" : (submitted ? "input staging timeout" : "boot timeout");
            break;
        }
        if (used == TRANSPORT_MAX) {
            error = "transport output limit";
            break;
        }
        struct pollfd p = { .fd = out[0], .events = POLLIN };
        int polled = poll(&p, 1, 50);
        if (polled < 0 && errno == EINTR) continue;
        if (polled < 0 || (p.revents & (POLLERR | POLLNVAL))) {
            error = "guest output poll failed";
            break;
        }
        if (p.revents & (POLLIN | POLLHUP)) {
            ssize_t n = read(out[0], buf + used, TRANSPORT_MAX - used);
            if (n > 0) {
                /* The trusted serial protocol is text; binary stdout is base64. */
                if (memchr(buf + used, 0, (size_t)n)) {
                    error = "NUL in guest control stream";
                    break;
                }
                used += (size_t)n;
                buf[used] = 0;
            } else if (n == 0) {
                error = "guest output closed early";
                break;
            } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                error = "guest output read failed";
                break;
            }
        }
        if (!ready && strstr(buf, "trynix: waiting for the store")) {
            if (send_guest(in[1], "ready\n")) {
                error = "guest handshake failed";
                break;
            }
            ready = 1;
        }
        if (!prepared && strstr(buf, "trynix: welcome to the multiverse")) {
            const char *setup =
                "exec /bin/sh -c 'if set +m && stty -echo -icanon; then "
                "printf \"INPUT_READY_%s\\n\" MARK; "
                "IFS= read -r command && eval \"$command\"; "
                "else printf \"SETUP_ERROR_%s\\n\" MARK; fi'\n";
            if (send_guest(in[1], setup)) {
                error = "guest setup write failed";
                break;
            }
            prepared = 1;
            used = 0;
            buf[0] = 0;
        }
        if (prepared && complete_line(buf, "SETUP_ERROR_MARK")) {
            error = "guest setup failed";
            break;
        }
        if (!submitted && prepared && complete_line(buf, "INPUT_READY_MARK")) {
            char *command = guest_command();
            if (!command) {
                error = "command allocation failed";
                break;
            }
            int failed = send_guest(in[1], command);
            free(command);
            if (failed) {
                error = "guest command write failed";
                break;
            }
            used = 0;
            buf[0] = 0;
            submitted = 1;
            deadline = now() + STAGE_SECONDS;
        }
        if (submitted && !executing && complete_line(buf, "EXEC_RUNNING_MARK")) {
            executing = 1;
            deadline = now() + EXEC_SECONDS;
            pthread_mutex_lock(&lock);
            j.state = JOB_RUNNING;
            pthread_mutex_unlock(&lock);
        }
        if (submitted) {
            int parsed = parse_result(buf, output, &output_n, &exit_code, &error);
            if (parsed < 0) break;
            if (parsed > 0) {
                done = 1;
                terminal = JOB_COMPLETE;
                break;
            }
        }
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            child_reaped = 1;
            pid = -1;
            error = "guest exited early";
            break;
        }
        if (waited < 0 && errno != EINTR) {
            error = "waitpid failed";
            break;
        }
    }

cleanup:
    if (actions_initialized && posix_spawn_file_actions_destroy(&actions)) {
        if (!error) error = "spawn actions destruction failed";
    }
    if (pid > 0) {
        child_reaped = terminate_and_reap(pid, &status) == 0;
        fprintf(stderr, "job %u reaped=%d status=%d\n", j.id, child_reaped, status);
        if (!child_reaped) {
            error = "child reap failed; instance quarantined";
            terminal = JOB_FAILED;
        }
    }
    for (int n = 0; n < 2; ++n) {
        if (in[n] >= 0) close(in[n]);
        if (out[n] >= 0) close(out[n]);
    }
    free(buf);
    pthread_mutex_lock(&lock);
    if (child_reaped && (j.cancel || shutdown_requested)) {
        terminal = JOB_CANCELLED;
        error = NULL;
    }
    j.reaped = child_reaped;
    j.exit_code = done ? exit_code : -1;
    if (done) {
        memcpy(j.output, output, output_n);
        j.output_n = output_n;
    }
    if (error) snprintf(j.error, sizeof j.error, "%s", error);
    j.state = terminal;
    active = !child_reaped;
    pthread_mutex_unlock(&lock);
    free(output);
}

static void *worker(void *unused)
{
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&lock);
        while (j.state != JOB_BOOTING && !shutdown_requested) {
            pthread_cond_wait(&changed, &lock);
        }
        int stop = shutdown_requested;
        pthread_mutex_unlock(&lock);
        if (stop) return NULL;
        run_job();
    }
}

static void reply(int c, int code, const char *body)
{
    char header[300];
    int n = snprintf(header, sizeof header,
                     "HTTP/1.1 %d Result\r\nContent-Type: application/json\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n"
                     "Content-Length: %zu\r\n\r\n", code, strlen(body));
    if (n < 0 || (size_t)n >= sizeof header) return;
    double deadline = now() + IO_SECONDS;
    if (!write_until(c, header, (size_t)n, deadline, 0)) {
        write_until(c, body, strlen(body), deadline, 0);
    }
}

/* Public mode exposes only synchronous execution: results never depend on
 * routing a subsequent request to this process. The response owns its socket
 * and prevents job replacement until output is copied and delivered/discarded.
 * Admission counters are process-local, NOT an account-wide spending limit.
 */
static void *synchronous_response(void *opaque)
{
    int c = *(int *)opaque;
    free(opaque);
    int disconnected = 0;
    double deadline = now() + BOOT_SECONDS + STAGE_SECONDS + EXEC_SECONDS + 2 * IO_SECONDS + REAP_SECONDS;
    for (;;) {
        pthread_mutex_lock(&lock);
        int terminal = j.state >= JOB_COMPLETE;
        int stop = shutdown_requested;
        pthread_mutex_unlock(&lock);
        if (terminal) break;
        struct pollfd p = { .fd = c, .events = POLLIN };
        int r = poll(&p, 1, 50);
        if (r < 0 && errno == EINTR) continue;
        if (stop || now() >= deadline || r < 0 ||
            (p.revents & (POLLHUP | POLLERR | POLLNVAL | POLLIN))) {
            /* No further request bytes are valid after Content-Length.
             * POLLIN also reports peer EOF. Either cancels this request.
             */
            disconnected = 1;
            pthread_mutex_lock(&lock);
            j.cancel = 1;
            pthread_mutex_unlock(&lock);
            break;
        }
    }
    if (!disconnected) {
        pthread_mutex_lock(&lock);
        char *encoded = b64(j.output, j.output_n);
        size_t cap = encoded ? strlen(encoded) + 400 : 0;
        char *body = encoded ? malloc(cap) : NULL;
        if (body) snprintf(body, cap,
            "{\"state\":%d,\"exit_code\":%d,\"reaped\":%s,"
            "\"error\":\"%s\",\"output_base64\":\"%s\"}",
            j.state, j.exit_code, j.reaped ? "true" : "false", j.error, encoded);
        int code = j.state == JOB_TIMEOUT ? 504 : (j.state == JOB_FAILED ? 502 : 200);
        pthread_mutex_unlock(&lock);
        if (body) reply(c, code, body);
        else reply(c, 500, "{\"error\":\"allocation failed\"}");
        free(body);
        free(encoded);
    }
    close(c);
    pthread_mutex_lock(&lock);
    response_pending = 0;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
    return NULL;
}

struct request {
    char *method, *path;
    size_t content, source;
    int have_content, have_source;
};

static int decimal_size(const char *value, size_t *result)
{
    while (*value == ' ' || *value == '\t') ++value;
    if (*value < '0' || *value > '9') return -1;
    size_t n = 0;
    while (*value >= '0' && *value <= '9') {
        unsigned digit = (unsigned)(*value++ - '0');
        if (n > (SIZE_MAX - digit) / 10) return -1;
        n = n * 10 + digit;
    }
    while (*value == ' ' || *value == '\t') ++value;
    if (*value) return -1;
    *result = n;
    return 0;
}

/* Parses only the bounded header region, never source/stdin bytes. */
static int parse_headers(char *buf, size_t header_n, struct request *r)
{
    if (header_n > HEADER_MAX || memchr(buf, 0, header_n)) return -1;
    char *line = strstr(buf, "\r\n");
    if (!line) return -1;
    *line = 0;
    char *space = strchr(buf, ' ');
    if (!space || space == buf) return -1;
    *space = 0;
    r->method = buf;
    r->path = space + 1;
    space = strchr(r->path, ' ');
    if (!space || space == r->path || strcmp(space + 1, "HTTP/1.1")) return -1;
    *space = 0;

    char *p = line + 2;
    char *headers_end = buf + header_n;
    while (p < headers_end - 2) {
        line = strstr(p, "\r\n");
        if (!line || line >= headers_end || line == p) return -1;
        *line = 0;
        char *colon = strchr(p, ':');
        if (!colon || colon == p) return -1;
        for (char *q = p; q < colon; ++q) {
            unsigned char ch = (unsigned char)*q;
            if (!isalnum(ch) && !strchr("!#$%&'*+-.^_`|~", ch)) return -1;
        }
        *colon = 0;
        char *value = colon + 1;
        for (char *q = value; *q; ++q) {
            unsigned char ch = (unsigned char)*q;
            if ((ch < 32 && ch != '\t') || ch == 127) return -1;
        }
        if (!strcasecmp(p, "Content-Length")) {
            if (r->have_content++ || decimal_size(value, &r->content)) return -1;
        } else if (!strcasecmp(p, "X-Source-Length")) {
            if (r->have_source++ || decimal_size(value, &r->source)) return -1;
        } else if (!strcasecmp(p, "Transfer-Encoding") || !strcasecmp(p, "Expect")) {
            return -1;
        }
        p = line + 2;
    }
    return p == headers_end - 2 && p[0] == '\r' && p[1] == '\n' ? 0 : -1;
}

/* Exactly /jobs/<unsigned id> or /jobs/<unsigned id>/cancel. */
static int job_route(const char *path, int cancel, unsigned *id)
{
    if (strncmp(path, "/jobs/", 6)) return -1;
    const char *p = path + 6;
    if (*p < '0' || *p > '9') return -1;
    unsigned value = 0;
    do {
        unsigned digit = (unsigned)(*p++ - '0');
        if (value > (UINT_MAX - digit) / 10) return -1;
        value = value * 10 + digit;
    } while (*p >= '0' && *p <= '9');
    if (!value || strcmp(p, cancel ? "/cancel" : "")) return -1;
    *id = value;
    return 0;
}

static int receive_more(int c, char *buf, size_t *n, size_t cap, double deadline)
{
    while (now() < deadline && *n < cap) {
        struct pollfd p = { .fd = c, .events = POLLIN };
        int ready = poll(&p, 1, 100);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || (p.revents & (POLLERR | POLLNVAL))) return -1;
        if (!ready) continue;
        ssize_t got = read(c, buf + *n, cap - *n);
        if (got > 0) {
            *n += (size_t)got;
            buf[*n] = 0;
            return 0;
        }
        if (got < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
        return -1;
    }
    return -1;
}

static int handle(int c)
{
    char *buf = calloc(1, REQUEST_MAX + 1);
    if (!buf) {
        reply(c, 500, "{\"error\":\"allocation failed\"}");
        return 0;
    }
    size_t n = 0, header_n = 0;
    double deadline = now() + 3;
    while (n < HEADER_MAX && now() < deadline) {
        if (receive_more(c, buf, &n, HEADER_MAX, deadline)) break;
        char *end = strstr(buf, "\r\n\r\n");
        if (end) {
            header_n = (size_t)(end - buf) + 4;
            break;
        }
    }
    struct request r = {0};
    if (!header_n || parse_headers(buf, header_n, &r)) {
        reply(c, 400, "{\"error\":\"bad headers\"}");
        goto finish;
    }
    int synchronous = !strcmp(r.method, "POST") && !strcmp(r.path, "/execute");
    int run = synchronous || (!public_poc && !strcmp(r.method, "POST") && !strcmp(r.path, "/run"));
    if (synchronous && r.have_content && !r.have_source) {
        /* Plain curl --data-binary source.lua: absent stdin split means all source. */
        r.source = r.content;
        r.have_source = 1;
    }
    if (run && (!r.have_content || !r.have_source)) {
        reply(c, 400, "{\"error\":\"length headers required\"}");
        goto finish;
    }
    if (run && (r.source > INPUT_MAX || r.content < r.source ||
                r.content - r.source > INPUT_MAX)) {
        reply(c, 413, "{\"error\":\"input limit\"}");
        goto finish;
    }
    if ((!run && (r.content || r.have_source)) ||
        r.content > REQUEST_MAX - header_n || n > header_n + r.content) {
        reply(c, 400, "{\"error\":\"unexpected body\"}");
        goto finish;
    }
    while (n < header_n + r.content) {
        if (receive_more(c, buf, &n, header_n + r.content, deadline)) break;
    }
    if (n != header_n + r.content) {
        reply(c, 400, "{\"error\":\"short body\"}");
        goto finish;
    }
    if (!strcmp(r.method, "GET") && !strcmp(r.path, "/health")) {
        reply(c, 200, public_poc ? "{\"status\":\"experimental-public-poc\"}" : "{\"status\":\"local-only\"}");
        goto finish;
    }
    if (run && public_expired()) {
        reply(c, 410, "{\"error\":\"POC expired\"}");
        goto finish;
    }
    if (run) {
        pthread_mutex_lock(&lock);
        if (active || response_pending || j.id == UINT_MAX ||
            (public_poc && (accepted_jobs >= PUBLIC_JOB_LIMIT || now() < next_admission))) {
            pthread_mutex_unlock(&lock);
            reply(c, 429, "{\"error\":\"busy\"}");
            goto finish;
        }
        int *owned_socket = synchronous ? malloc(sizeof *owned_socket) : NULL;
        if (synchronous && !owned_socket) {
            pthread_mutex_unlock(&lock);
            reply(c, 500, "{\"error\":\"allocation failed\"}");
            goto finish;
        }
        unsigned id = j.id + 1;
        memset(&j, 0, sizeof j);
        j.id = id;
        j.state = JOB_BOOTING;
        j.exit_code = -1;
        j.source_n = r.source;
        j.stdin_n = r.content - r.source;
        memcpy(j.source, buf + header_n, j.source_n);
        memcpy(j.input, buf + header_n + r.source, j.stdin_n);
        active = 1;
        ++accepted_jobs;
        next_admission = now() + PUBLIC_ADMISSION_SECONDS;
        response_pending = synchronous;
        if (synchronous) {
            *owned_socket = c;
            pthread_t responder;
            if (pthread_create(&responder, NULL, synchronous_response, owned_socket)) {
                free(owned_socket);
                active = response_pending = 0;
                j.state = JOB_FAILED;
                pthread_mutex_unlock(&lock);
                reply(c, 500, "{\"error\":\"response thread failed\"}");
                goto finish;
            }
            pthread_detach(responder);
        }
        pthread_cond_signal(&changed);
        pthread_mutex_unlock(&lock);
        if (synchronous) { free(buf); return 1; }
        char body[80];
        snprintf(body, sizeof body, "{\"id\":%u}", id);
        reply(c, 202, body);
        goto finish;
    }
    if (public_poc) { reply(c, 404, "{}"); goto finish; }
    unsigned id = 0;
    int cancel = !strcmp(r.method, "POST");
    if ((!cancel && strcmp(r.method, "GET")) || job_route(r.path, cancel, &id)) {
        reply(c, 404, "{}");
        goto finish;
    }
    pthread_mutex_lock(&lock);
    if (id != j.id) {
        pthread_mutex_unlock(&lock);
        reply(c, 404, "{}");
        goto finish;
    }
    if (cancel && active) j.cancel = 1;
    char *encoded = b64(j.output, j.output_n);
    size_t cap = encoded ? strlen(encoded) + 400 : 0;
    char *body = encoded ? malloc(cap) : NULL;
    if (body) {
        snprintf(body, cap,
                 "{\"id\":%u,\"state\":%d,\"exit_code\":%d,\"reaped\":%s,"
                 "\"error\":\"%s\",\"output_base64\":\"%s\"}",
                 j.id, j.state, j.exit_code, j.reaped ? "true" : "false",
                 j.error, encoded);
    }
    pthread_mutex_unlock(&lock);
    if (body) reply(c, cancel ? 202 : 200, body);
    else reply(c, 500, "{\"error\":\"allocation failed\"}");
    free(body);
    free(encoded);
finish:
    free(buf);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    childargs = argv + 1;
    const char *mode = getenv("PUBLIC_POC");
    public_poc = mode && !strcmp(mode, "1");
    if (public_poc) {
        const char *expiry = getenv("POC_EXPIRES_AT");
        size_t value;
        struct timespec t;
        if (!expiry || decimal_size(expiry, &value) || !value ||
            clock_gettime(CLOCK_REALTIME, &t) || t.tv_sec < 0 ||
            (uint64_t)value > (uint64_t)t.tv_sec + 3600) {
            fprintf(stderr, "PUBLIC_POC requires POC_EXPIRES_AT within the next hour\n");
            return 2;
        }
        public_expires_at = value;
    }
    const char *port_text = getenv("PORT");
    size_t port = public_poc ? 80 : 18081;
    if (port_text && (decimal_size(port_text, &port) || port == 0 || port > 65535)) return 2;
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) return 3;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
        .sin_addr.s_addr = htonl(public_poc ? INADDR_ANY : INADDR_LOOPBACK)
    };
    if (listener < 0 || fd_flags(listener, 1) ||
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) ||
        bind(listener, (void *)&addr, sizeof addr) || listen(listener, 8)) {
        perror("listen");
        if (listener >= 0) close(listener);
        return 3;
    }
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, NULL)) {
        close(listener);
        return 4;
    }
    fprintf(stderr, "Lua API ready port=%zu mode=%s\n", port, public_poc ? "public-poc" : "local");
    double deadline = now() + 600;
    while (now() < deadline) {
        struct pollfd p = { .fd = listener, .events = POLLIN };
        int ready = poll(&p, 1, 100);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || (p.revents & (POLLERR | POLLHUP | POLLNVAL))) break;
        if (!ready) continue;
        int c = accept(listener, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }
        int transferred = !fd_flags(c, 1) && handle(c);
        if (!transferred) close(c);
    }
    pthread_mutex_lock(&lock);
    shutdown_requested = 1;
    j.cancel = 1;
    pthread_cond_signal(&changed);
    pthread_mutex_unlock(&lock);
    pthread_join(thread, NULL);
    pthread_mutex_lock(&lock);
    while (response_pending) pthread_cond_wait(&changed, &lock);
    pthread_mutex_unlock(&lock);
    close(listener);
    return 0;
}
