// Stress test for loop bodies that themselves spawn: a cilk_for whose body
// runs `x = cilk_spawn fib(a); y = fib(b); cilk_sync;`.  Frames called from the
// body have the loop frame as call_parent (the inner loop frame is not a
// spawner), so a steal inside the body promotes frames up to the loop frame.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../runtime/cilk2c.h"
// hand-compiled code needs the ABI functions inlined (cilkify cannot happen
// inside a non-inlined function)
#include "../runtime/cilk2c_inlined.c"

extern size_t ZERO;
void __attribute__((weak)) dummy(void *p) { return; }

static int fib_serial(int n) { return n < 2 ? n : fib_serial(n - 1) + fib_serial(n - 2); }

static void __attribute__((noinline))
fib_spawn_helper(int *x, int n, __cilkrts_stack_frame *parent);

static int fib(int n) {
    int x = 0, y, _tmp;
    if (n < 2)
        return n;

    dummy(alloca(ZERO));
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);
    if (!__cilk_prepare_spawn(&sf)) {
        fib_spawn_helper(&x, n - 1, &sf);
    }
    y = fib(n - 2);
    __cilk_sync_nothrow(&sf);
    _tmp = x + y;
    __cilk_parent_epilogue(&sf);
    return _tmp;
}

static void __attribute__((noinline))
fib_spawn_helper(int *x, int n, __cilkrts_stack_frame *parent) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_helper(&sf, parent, false);
    __cilkrts_detach(&sf, parent);
    *x = fib(n);
    __cilk_helper_epilogue(&sf, parent, false);
}

typedef struct {
    int *res;
    int base;
} data_t;

static int iter_n(const data_t *d, uint64_t i) { return d->base + (int)(i % 7); }

static void __attribute__((noinline))
iteration_spawn_helper(int *x, int n, __cilkrts_stack_frame *parent);

// res[i] = fib(n_i) + fib(n_i - 1), computed with a spawn and a sync
static void iteration(const data_t *d, uint64_t i) {
    int x = 0, y;
    int n = iter_n(d, i);

    dummy(alloca(ZERO));
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);
    if (!__cilk_prepare_spawn(&sf)) {
        iteration_spawn_helper(&x, n, &sf);
    }
    y = fib(n - 1);
    __cilk_sync_nothrow(&sf);
    d->res[i] = x + y;
    __cilk_parent_epilogue(&sf);
}

static void __attribute__((noinline))
iteration_spawn_helper(int *x, int n, __cilkrts_stack_frame *parent) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_helper(&sf, parent, false);
    __cilkrts_detach(&sf, parent);
    *x = fib(n);
    __cilk_helper_epilogue(&sf, parent, false);
}

static void body(void *data, uint64_t low, uint64_t high) {
    for (uint64_t i = low; i < high; ++i)
        iteration(data, i);
}

// Outer loop whose body runs the spawning inner loop, to nest loop frames
// inside bodies that spawn.
typedef struct {
    data_t *d;
    int width;
} outer_t;

static void inner_loop(data_t *d, int width, int offset) {
    __cilkrts_cilk_for_64(body, &(data_t){.res = d->res + offset, .base = d->base},
                          width, 1);
}

static void outer_body(void *data, uint64_t low, uint64_t high) {
    outer_t *o = data;
    for (uint64_t i = low; i < high; ++i)
        inner_loop(o->d, o->width, (int)i * o->width);
}

int main(int argc, char *argv[]) {
    int n = argc > 1 ? atoi(argv[1]) : 256;
    int base = argc > 2 ? atoi(argv[2]) : 14;
    unsigned grain = argc > 3 ? atoi(argv[3]) : 1;
    int *res = calloc(n, sizeof(int));
    int *res2 = calloc(n, sizeof(int));
    data_t d = {.res = res, .base = base};

    __cilkrts_cilk_for_64(body, &d, n, grain);

    // nested: 16 outer iterations, each running n/16 spawning inner iterations
    data_t d2 = {.res = res2, .base = base};
    outer_t o = {.d = &d2, .width = n / 16};
    __cilkrts_cilk_for_64(outer_body, &o, 16, 1);

    int bad = 0;
    for (int i = 0; i < n; i++) {
        int m = base + i % 7;
        int expect = fib_serial(m) + fib_serial(m - 1);
        if (res[i] != expect) bad++;
        if (i < (n / 16) * 16 && res2[i] != base * 0 + fib_serial(base + (i % (n / 16)) % 7) + fib_serial(base + (i % (n / 16)) % 7 - 1))
            bad++;
    }
    if (bad) {
        fprintf(stderr, "loop_spawn: %d wrong results\n", bad);
        return 1;
    }
    printf("loop_spawn OK\n");
    return 0;
}
