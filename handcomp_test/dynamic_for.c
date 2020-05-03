//
// Created by luka on 4/5/20.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"
#include "../runtime/scheduler.h"
#include "getoptions.h"

#include "ktiming.h"

#ifndef TIMING_COUNT
#define TIMING_COUNT 1
#endif
/*
 * void daxpy(int n, double a, double *x, double *y) {
 *   cilk_for (int i = 0; i < n; ++i) {
 *     y[i] = a * x[i] + y[i];
 *   }
 * }
 *
 */


// we cannot inline this function because of local variables
static void __attribute__ ((noinline)) daxpy_loop_helper(double *y, const double *x, double a) {
    __uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_init_inner_loop_frame(&inner_lf);

    __cilkrts_iteration_return status = __cilkrts_grab_iteration(&inner_lf, &i);
    while (status != FAIL) {
        __cilkrts_enter_inner_loop_frame(&inner_lf);
        if (status != SUCCESS_LAST_ITERATION) {
            __cilkrts_detach(&inner_lf.sf);
            // push the parent loop_frame to the deque
        }

        // LOOP BODY
        y[i] += a * x[i];
        // END OF LOOP BODY

        __cilkrts_pop_frame(&inner_lf.sf);
        __cilkrts_leave_frame(&inner_lf.sf);
        // if this returns, we've obtained the loop frame

        if(status != SUCCESS_LAST_ITERATION) {
            status = __cilkrts_grab_iteration(&inner_lf, &i);
        } else {
            break; // if we were in our last iteration, we're done
        }
    }

    // here, we have left the frame and didn't reenter it.
}

void daxpy(double *y, double *x, double a, uint64_t n) {

    __cilkrts_loop_frame lf;
    __cilkrts_enter_loop_frame(&lf, 0, n);

    // cilk_for(int i = 0; i < n; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    if (!__builtin_setjmp(lf.sf.ctx)) {
        // first time entering this loop,
        // else is entering the loop after steal
        // make sure the setjmp doesn't get optimized away.

        __cilkrts_get_tls_worker()->local_loop_frame = &lf;

    } else {

    }

    daxpy_loop_helper(y, x, a);

    if(__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if(!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }

    __cilkrts_pop_frame(&local_lf()->sf);
    __cilkrts_leave_loop_frame(local_lf());

    //outer loop frame end scope
}

int usage(void) {
    fprintf(stderr,
            "\nUsage: dynamic_for [-n size] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-n", "-c", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, BOOLARG, 0};

int cilk_main(int argc, char *argv[]) {

    double a = 3.0;

    uint64_t N = 1000000;
    int help = 0, check = 0;

    get_options(argc, argv, specifiers, opt_types, &N, &check, &help);

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

    for(int i = 0; i < TIMING_COUNT; i++) {
        begin = ktiming_getmark();
        daxpy(y, x, a, N);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);

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