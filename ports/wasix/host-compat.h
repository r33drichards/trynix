#ifndef TRYNIX_WASIX_HOST_COMPAT_H
#define TRYNIX_WASIX_HOST_COMPAT_H
#include <errno.h>
#include <sys/types.h>
/* WASIX EH mode does not implement host fork. Guest Linux fork is unrelated.
 * GLib subprocess/debugger helpers must fail explicitly, never fake success. */
#if defined(__wasi__) && defined(__wasm_exception_handling__)
static inline pid_t trynix_wasix_host_fork(void) { errno = ENOSYS; return -1; }
#define fork trynix_wasix_host_fork
#endif
#endif
