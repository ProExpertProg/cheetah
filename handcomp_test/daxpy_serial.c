//
// Created by luka on 5/2/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


void daxpy(double *y, double *x, double a, uint64_t n) {
    for (int i = 0; i < n; ++i) {
        y[i] = a * x[i] + y[i];
    }
}

int usage(void) {
    fprintf(stderr,
            "\nUsage: daxpy [-n size] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-n", "-c", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, BOOLARG, 0};

int main(int argc, char *argv[]) {

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

    for (int t = 0; t < TIMING_COUNT; t++) {
        begin = ktiming_getmark();
        daxpy(y, x, a, N);
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
            if (success)
                printf("Successful\n");
        }
        memset(y, 0, N * sizeof(double));
    }

    print_runtime(running_time, TIMING_COUNT);

    free(y);
    free(x);

    return 0;
}