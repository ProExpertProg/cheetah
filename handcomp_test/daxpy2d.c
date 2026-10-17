// Nested cilk_for: M rows of daxpy. Every outer chunk runs many inner loops, so
// the loop-frame sync/steal code runs often.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getoptions.h"
#include "ktiming.h"
#include "../runtime/cilk2c.h"

typedef struct {
    double *y, *x, a;
    uint64_t n, inner_grain;
} data_t;

static void inner_body(void *d, uint64_t low, uint64_t high) {
    const data_t *data = d;
    for (uint64_t i = low; i < high; ++i) {
        data->y[i] += data->x[i] * data->a;
    }
}

static void outer_body(void *d, uint64_t low, uint64_t high) {
    const data_t *data = d;
    for (uint64_t r = low; r < high; ++r) {
        data_t row = *data;
        row.x += r * data->n;
        row.y += r * data->n;
        __cilkrts_cilk_for_64(inner_body, &row, data->n, data->inner_grain);
    }
}

int usage(void) {
    fprintf(stderr, "\nUsage: daxpy2d [-m rows] [-n columns] [-g inner grainsize] [-o outer grainsize] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-m", "-n", "-g", "-o", "-c", "-h", 0};
int opt_types[] = {LONGARG, LONGARG, LONGARG, LONGARG, BOOLARG, BOOLARG, 0};

int main(int argc, char *argv[]) {
    long M = 4096, N = 4096, inner_grain = 1, outer_grain = 1;
    int check = 0, help = 0;

    get_options(argc, argv, specifiers, opt_types, &M, &N, &inner_grain, &outer_grain, &check, &help);
    if (help)
        return usage();

    double *y = calloc(M * N, sizeof(double));
    double *x = malloc(M * N * sizeof(double));
    srand(0);
    for (long i = 0; i < M * N; ++i)
        x[i] = rand() % 1000;

    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];
    for (int t = 0; t < TIMING_COUNT; t++) {
        data_t data = {.y = y, .x = x, .a = 3.0, .n = N, .inner_grain = inner_grain};
        begin = ktiming_getmark();
        __cilkrts_cilk_for_64(outer_body, &data, M, outer_grain);
        end = ktiming_getmark();
        running_time[t] = ktiming_diff_nsec(&begin, &end);
        if (check) {
            int success = 1;
            for (long i = 0; i < M * N; ++i)
                if (x[i] * 3.0 != y[i]) {
                    printf("Discrepancy at i=%ld\n", i);
                    success = 0;
                    break;
                }
            if (success)
                printf("Successful\n");
        }
        memset(y, 0, M * N * sizeof(double));
    }
    print_runtime(running_time, TIMING_COUNT);
    free(y);
    free(x);
    return 0;
}
