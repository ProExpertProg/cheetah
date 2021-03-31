//
// Created by luka on 5/2/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"
#include "../runtime/cilk2c_inlined.c"
#include "../runtime/scheduler.h"
#include "getoptions.h"
#include "ktiming.h"

/*
 * void daxpy(int n, double a, double *x, double *y) {
 *   cilk_for (int i = 0; i < n; ++i) {
 *     y[i] = a * x[i] + y[i];
 *   }
 * }
 *
 */

typedef void (*ForBody)(uint64_t i, void *data);

typedef struct {
    double *y, *x, a;
} data;

static void __attribute__ ((noinline)) cilk_loop_helper(uint64_t low, uint64_t high, void *data, uint64_t grainsize);

void cilk_loop_recursive(uint64_t low, uint64_t high, void *d, uint64_t grainsize) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);


    uint64_t len = high - low;
    while (len > grainsize) {
        uint64_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        __cilkrts_save_fp_ctrl_state(&sf);
        if(!__builtin_setjmp(sf.ctx)) {
            cilk_loop_helper(low, mid, d, grainsize);
        }

        low = mid;
        len = high - low;
    }

    for (int i = low; i < high; ++i) {
        // body
        data *_d = (data *) d;
        _d->y[i] += _d->a * _d->x[i];
    }

    /* cilk_sync */
    if(sf.flags & CILK_FRAME_UNSYNCHED) {
        __cilkrts_save_fp_ctrl_state(&sf);
        if(!__builtin_setjmp(sf.ctx)) {
            __cilkrts_sync(&sf);
        }
    }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

}

// we cannot inline this function because of local variables
static void __attribute__ ((noinline)) cilk_loop_helper(uint64_t low, uint64_t high, void *data, uint64_t grainsize) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);
    __cilkrts_detach(&sf);
    cilk_loop_recursive(low, high, data, grainsize);
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

void daxpy(double *y, double *x, double a, uint64_t n, uint64_t grainsize) {
    data d = {.a=a, .x=x, .y=y};

    cilk_loop_recursive(0, n, &d, grainsize);
}

int usage(void) {
    fprintf(stderr,
            "\nUsage: daxpy [-n size] [-c] [-h] [-g grainsize]\n\n");
    return -1;
}

const char *specifiers[] = {"-n", "-c", "-g", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, LONGARG, BOOLARG, 0};

int cilk_main(int argc, char *argv[]) {

    double a = 3.0;

    uint64_t N = 1000000;
    uint64_t grainsize = 1;
    int help = 0, check = 0;

    get_options(argc, argv, specifiers, opt_types, &N, &check, &grainsize, &help);

    if (help) {
        return usage();
    }

    double *y = calloc(N, sizeof(double));
    double *x = malloc(N * sizeof(double));

    srand(0);

    for (int i = 0; i < N; ++i) {
        x[i] = rand() % 1000;
    }

    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    for(int t = 0; t < TIMING_COUNT; t++) {
        begin = ktiming_getmark();
        daxpy(y, x, a, N, grainsize);
        end = ktiming_getmark();
        running_time[t] = ktiming_diff_usec(&begin, &end);

        if (check) {
            int success = 1;
            for (int i = 0; i < N; ++i) {
                if (x[i] * a != y[i]) {
                    printf("Discrepancy at i=%d: x[i]=%f, y[i]=%f\n", i, x[i], y[i]);
                    success = 0;
                }
            }
            if(success)
                printf("Successful\n");
        }
        memset(y, 0, N * sizeof(double));
    }

    print_runtime(running_time, TIMING_COUNT);

    free(y);
    free(x);

    return 0;
}