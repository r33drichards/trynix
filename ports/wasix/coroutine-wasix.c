/* SPDX-License-Identifier: LGPL-2.0-or-later */
/* WASIX execution contexts replace the browser-only Emscripten fibers.
 * Contexts are thread-affine. Do not move a coroutine between host threads.
 *
 * Each coroutine owns a bounded linear-memory C-stack region. WASIX switches
 * the engine call stack only, so the guest __stack_pointer global must be
 * swapped at the switch boundary (see coroutine-wasix-switch.S); otherwise all
 * suspended coroutines share one stack and the module traps once it is
 * exhausted.
 */
#include "qemu/osdep.h"
#include "qemu/coroutine_int.h"
#include "qemu/coroutine-tls.h"
#include <wasix/context.h>
#include <pthread.h>

void *wasix_sp_get(void);
int wasix_sp_ctx_switch(void **save_slot, void *new_sp,
                        wasix_context_id_t target_context);

/* Same order of magnitude as the ucontext/sigaltstack backends. */
#ifndef COROUTINE_WASIX_STACK_SIZE
#define COROUTINE_WASIX_STACK_SIZE (256 * 1024)
#endif
#define COROUTINE_WASIX_STACK_ALIGN 16

typedef struct CoroutineWasix {
    Coroutine base;
    void *entry_arg_wasix;
    wasix_context_id_t context;
    CoroutineAction action;
    pthread_t owner;
    void *stack;        /* region base, freed with the coroutine */
    void *stack_top;    /* initial __stack_pointer for this coroutine */
    void *sp;           /* saved __stack_pointer while suspended */
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
    co->stack = g_malloc0(COROUTINE_WASIX_STACK_SIZE);
    co->sp = co->stack_top = (char *)co->stack + COROUTINE_WASIX_STACK_SIZE;
    co->sp = (void *)ROUND_UP((uintptr_t)co->sp, COROUTINE_WASIX_STACK_ALIGN);
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
    g_free(co->stack);
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
    if (wasix_sp_ctx_switch(&from->sp, to->sp, to->context)) {
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
            /* The leader keeps the stack it was started with. */
            co->sp = wasix_sp_get();
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
