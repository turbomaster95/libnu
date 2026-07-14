#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <nu.h>

#define NU_FIBER_STACK_SIZE (64 * 1024)

static nu_scheduler_t sched;

static void nu_fiber_bootstrap(void) {
    nu_fiber_t *curr = sched.current;
    curr->entry(curr->arg);

    // Mark as dead and yield back to the scheduler
    curr->state = NU_FIBER_DEAD;

    // swapcontext will be called inside nu_yield to move to the next fiber
    // and we never return here.
    nu_yield();
    unreachable();
}

void nu_sched_init(nu_mm_t* mm) {
    if (!mm) { return NULL; }

    memset(&sched, 0, sizeof(sched));

    sched.idle = nu_alloc(mm, sizeof(nu_fiber_t));
    sched.idle->state = NU_FIBER_RUNNING;
    sched.idle->stack = NULL;
    sched.idle->next = NULL;

    sched.current = sched.idle;
}

void nu_fiber_create(nu_mm_t* mm, void (*entry)(void *), void *arg) {
    if (!mm) { return NULL; }

    nu_fiber_t *fiber = nu_alloc(mm, sizeof(nu_fiber_t));

    fiber->state = NU_FIBER_READY;
    fiber->entry = entry;
    fiber->arg = arg;
    fiber->next = NULL;
    getcontext(&fiber->ctx);

    fiber->stack = nu_alloc(mm, NU_FIBER_STACK_SIZE);
    fiber->ctx.uc_stack.ss_sp = fiber->stack;
    fiber->ctx.uc_stack.ss_size = NU_FIBER_STACK_SIZE;
    fiber->ctx.uc_stack.ss_flags = 0;
    fiber->ctx.uc_link = &sched.idle->ctx;
    makecontext(&fiber->ctx, (void (*)(void))nu_fiber_bootstrap, 0);

    // Queue it up
    if (!sched.run_queue) {
        sched.run_queue = fiber;
        sched.run_queue_tail = fiber;
    } else {
        sched.run_queue_tail->next = fiber;
        sched.run_queue_tail = fiber;
    }
}

void nu_yield(void) {
    nu_fiber_t *prev = sched.current;

    // Search ready queue
    nu_fiber_t *next = sched.run_queue;
    while (next && next->state != NU_FIBER_READY) {
        next = next->next;
    }

    if (next) {
        if (prev->state == NU_FIBER_RUNNING) {
            prev->state = NU_FIBER_READY;
        }
        next->state = NU_FIBER_RUNNING;
        sched.current = next;
        swapcontext(&prev->ctx, &next->ctx);
    } else if (prev->state == NU_FIBER_DEAD && prev != sched.idle) {
        // Fall back to main execution stream if nothing left
        sched.current = sched.idle;
        setcontext(&sched.idle->ctx);
    }
}

void nu_sched_run(void) {
    while (1) {
        nu_fiber_t *it = sched.run_queue;
        int active_fibers = 0;
        while (it) {
            if (it->state == NU_FIBER_READY) active_fibers++;
            it = it->next;
        }

        if (active_fibers == 0) break;
        nu_yield();
    }
}
