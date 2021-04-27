//
// Created by luka on 4/5/20.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"
#include "../runtime/cilk2c_inlined.c"

#include "getoptions.h"
#include "ktiming.h"

extern size_t ZERO;

void __attribute__((weak)) dummy(void *p) { return; }

/*
 * void daxpy(int n, double a, double *x, double *y) {
 *   cilk_for (int i = 0; i < n; ++i) {
 *     y[i] = a * x[i] + y[i];
 *   }
 * }
 *
 */


// we cannot inline this function because of local variables
static void __attribute__ ((noinline)) daxpy_loop_helper(double *y, const double *x, double a, uint64_t grainsize) {
    __uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        do {
            // LOOP BODY
            for (uint64_t j = i * grainsize; j < (i + 1) * grainsize; j++) {
                y[j] += a * x[j];
            }
            // END OF LOOP BODY
            status = __cilkrts_loop_frame_next(&inner_lf);
            i += grainsize;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        // LOOP BODY
        for (uint64_t j = i * grainsize; j < (i + 1) * grainsize; j++) {
            y[j] += a * x[j];
        }
        // END OF LOOP BODY
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    __cilkrts_get_tls_worker()->local_loop_frame = (__cilkrts_loop_frame *) inner_lf.sf.call_parent;
    CILK_ASSERT(__cilkrts_get_tls_worker(), local_lf() == inner_lf.parentLF);

    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
}

void daxpy(double *y, double *x, double a, uint64_t n, uint64_t grainsize) {

    dummy(alloca(ZERO));
    __cilkrts_loop_frame lf;
    uint64_t end = n / grainsize, rem = n % grainsize;
    __cilkrts_enter_loop_frame(&lf, 0, end);

    // cilk_for(int i = 0; i < n; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    if (!__builtin_setjmp(lf.sf.ctx)) {
        // first time entering this loop,
        // else is entering the loop after steal
        // make sure the setjmp doesn't get optimized away.

        __cilkrts_get_tls_worker()->local_loop_frame = &lf;

    }

    daxpy_loop_helper(y, x, a, grainsize);

    CILK_ASSERT(__cilkrts_get_tls_worker(), local_lf()->start == local_lf()->end);

    if(__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if (!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }


    __cilkrts_pop_frame(&local_lf()->sf);
    // cannot refer to local_lf() for this check because w could be null
    if (__cilkrts_get_tls_worker() != NULL)
        __cilkrts_leave_loop_frame(local_lf());

    //outer loop frame end scope

    for (uint64_t i = n - rem; i < n; ++i) {
        y[i] += a * x[i];
    }
}

int usage(void) {
    fprintf(stderr,
            "\nUsage: daxpy [-n size] [-c] [-h] [-g grainsize]\n\n");
    return -1;
}

const char *specifiers[] = {"-n", "-c", "-g", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, LONGARG, BOOLARG, 0};

// easier debugging
double *x, *y;

int main(int argc, char *argv[]) {

    double a = 3.0;

    uint64_t N = 1000000;
    uint64_t grainsize = 1;
    int help = 0, check = 0;

    get_options(argc, argv, specifiers, opt_types, &N, &check, &grainsize, &help);

    if (help) {
        return usage();
    }

    y = calloc(N, sizeof(double));
    x = malloc(N * sizeof(double));

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
        running_time[t] = ktiming_diff_nsec(&begin, &end);

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