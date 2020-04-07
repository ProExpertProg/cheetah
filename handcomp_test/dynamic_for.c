//
// Created by luka on 4/5/20.
//
#include <stdio.h>
#include <stdlib.h>

#include "../runtime/cilk2c.h"
#include "../runtime/scheduler.h"
#include "getoptions.h"
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
    __cilkrts_enter_inner_loop_frame(&inner_lf);

    __cilkrts_pop_lf_return status = __cilkrts_grab_first_iteration(&inner_lf, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        __cilkrts_pop_lf_return pop_result;
        do {
            // LOOP BODY
            y[i] += a * x[i];
            // END OF LOOP BODY
            pop_result = __cilkrts_pop_loop_frame(&inner_lf, &i);
        } while (pop_result == SUCCESS_ITERATION);
    }
    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
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
    } else {

    }

    daxpy_loop_helper(y, x, a);

    // TODO sync

    // going to victim's stack should be fine

    __cilkrts_pop_frame(&lf.sf);
    __cilkrts_leave_frame(&lf.sf);

    //outer loop frame end scope
}

int usage(void) {
    fprintf(stderr,
            "\nUsage: cilksort [-n size] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-n", "-c", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, BOOLARG, 0};

int cilk_main(int argc, char *argv[]) {

    double a = 3.0;

    uint64_t N = 10000;
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

    daxpy(y, x, a, N);


    if (check) {
        for (int i = 0; i < N; ++i) {
            if (x[i] * a != y[i])
                printf("Dicrepancy at i=%f: x[i]=%f, y[i]=%f\n", x[i], y[i], a);
        }
    }

    free(y);
    free(x);

    return 0;
}