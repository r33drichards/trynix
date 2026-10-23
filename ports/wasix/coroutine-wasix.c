/* SPDX-License-Identifier: LGPL-2.0-or-later */
/* WASIX execution contexts replace the browser-only Emscripten fibers.
 * Contexts are thread-affine. Do not move a coroutine between host threads.
 */
#include "qemu/osdep.h"
#include "qemu/coroutine_int.h"
#include "qemu/coroutine-tls.h"
#include <wasix/context.h>
#include <pthread.h>

typedef struct CoroutineWasix {
    Coroutine base;
    wasix_context_id_t context;
    CoroutineAction action;
    pthread_t owner;
} CoroutineWasix;

QEMU_DEFINE_STATIC_CO_TLS(Coroutine *, current);
QEMU_DEFINE_STATIC_CO_TLS(CoroutineWasix *, leader);

static void coroutine_trampoline(void)
{
    Coroutine *co = get_current();
    for (;;) {
        co->entry(co->entry_arg);
        qemu_coroutine_switch(co, co->caller, COROUTINE_TERMINATE);
    }
}

Coroutine *qemu_coroutine_new(void)
{
    CoroutineWasix *co = g_new0(CoroutineWasix, 1);
    co->owner = pthread_self();
    if (wasix_context_create(&co->context, coroutine_trampoline)) {
        /* ENOTSUP means this runtime backend does not provide contexts. */
        perror("QEMU: WASIX context_create");
        abort();
    }
    return &co->base;
}

void qemu_coroutine_delete(Coroutine *base)
{
    CoroutineWasix *co = container_of(base, CoroutineWasix, base);
    if (!pthread_equal(co->owner, pthread_self())) {
        fprintf(stderr, "QEMU: WASIX coroutine crossed host threads\n");
        abort();
    }
    if (wasix_context_destroy(co->context)) {
        perror("QEMU: WASIX context_destroy");
        abort();
    }
    g_free(co);
}

CoroutineAction qemu_coroutine_switch(Coroutine *from_base, Coroutine *to_base,
                                     CoroutineAction action)
{
    CoroutineWasix *from = container_of(from_base, CoroutineWasix, base);
    CoroutineWasix *to = container_of(to_base, CoroutineWasix, base);
    if (!pthread_equal(from->owner, pthread_self()) ||
        !pthread_equal(to->owner, pthread_self())) {
        fprintf(stderr, "QEMU: WASIX coroutine crossed host threads\n");
        abort();
    }
    set_current(to_base);
    to->action = action;
    if (wasix_context_switch(to->context)) {
        perror("QEMU: WASIX context_switch");
        abort();
    }
    return from->action;
}

Coroutine *qemu_coroutine_self(void)
{
    Coroutine *self = get_current();
    if (!self) {
        CoroutineWasix *co = get_leader();
        if (!co) {
            co = g_new0(CoroutineWasix, 1);
            co->context = wasix_context_main;
            co->owner = pthread_self();
            set_leader(co);
        }
        self = &co->base;
        set_current(self);
    }
    return self;
}

bool qemu_in_coroutine(void)
{
    Coroutine *self = get_current();
    return self && self->caller;
}
