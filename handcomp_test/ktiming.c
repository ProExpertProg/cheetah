/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

/**
 * Linux kernel-assisted timing library -- provides high-precision time
 * measurements for the execution time of your algorithms.
 *
 * You shouldn't need to modify this file. More importantly, you should not
 * depend on any modifications you make here, as we will replace it with a
 * fresh copy when we test your code.
 **/

#include "./ktiming.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NSEC_TO_SEC(x) ((double)(x)*1.0e-9)

clockmark_t ktiming_getmark(void) {
    struct timespec temp = {0, 0};
    uint64_t nanos;

    int stat = clock_gettime(CLOCK_MONOTONIC, &temp);
    if (stat != 0) {
        perror("ktiming_getmark()");
        exit(-1);
    }
    nanos = temp.tv_nsec;
    nanos += ((uint64_t)temp.tv_sec) * 1000 * 1000 * 1000;
    return nanos;
}

uint64_t ktiming_diff_nsec(const clockmark_t *const start,
                           const clockmark_t *const end) {
    return *end - *start;
}

double ktiming_diff_sec(const clockmark_t *const start,
                        const clockmark_t *const end) {
    return NSEC_TO_SEC(ktiming_diff_nsec(start, end));
}

int cmp_uint64_t(const void *a, const void *b) {
    return (int) (*(uint64_t *) a - *(uint64_t *) b);
}

static void print_runtime_helper(uint64_t *nsec_elapsed, int size,
                                 int summary) {

    int i;
    uint64_t total = 0, sq_total = 0;
    double ave, std_dev = 0, median, min_t = nsec_elapsed[0];

    for (i = 0; i < size; i++) {
        total += nsec_elapsed[i];
        if (size > 1) {
            sq_total += nsec_elapsed[i] * nsec_elapsed[i] / (size - 1);
        }
        if(nsec_elapsed[i] < min_t)
            min_t = nsec_elapsed[i];

        if (!summary) {
            printf("Running time %d: %gs\n", (i + 1),
                   NSEC_TO_SEC(nsec_elapsed[i]));
        }
    }
    ave = total / size;

    if (size > 1) {
        // sqrt( <x^2> - <x>^2)
        std_dev = sqrt(sq_total - total  / (size - 1) * ave);
    }

    qsort(nsec_elapsed, TIMING_COUNT, sizeof(uint64_t), cmp_uint64_t);

    int mid_i = TIMING_COUNT / 2;

    if (TIMING_COUNT % 2) {
        median = nsec_elapsed[mid_i];
    } else {
        median = (nsec_elapsed[mid_i - 1] + nsec_elapsed[mid_i]) / 2.0;
    }

    printf("\nRunning time average: %g s\n", NSEC_TO_SEC(ave));
    printf("Running time median:  %g s\n", NSEC_TO_SEC(median));
    printf("Running time minimum: %g s\n\n", NSEC_TO_SEC(min_t));

    if (std_dev != 0) {
        printf("Std. dev: %g s (%2.3f%%)\n",
               NSEC_TO_SEC(std_dev), 100.0 * std_dev / ave);
    }
}

void print_runtime(uint64_t *tm_elapsed, int size) {
    print_runtime_helper(tm_elapsed, size, 0);
}

void print_runtime_summary(uint64_t *tm_elapsed, int size) {
    print_runtime_helper(tm_elapsed, size, 1);
}
