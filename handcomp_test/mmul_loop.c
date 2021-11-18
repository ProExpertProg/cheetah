#include <stdio.h>
#include <stdlib.h>

#include "../runtime/cilk2c.h"
#include "ktiming.h"
#include "getoptions.h"
#include "cilk_for.h"

#ifndef TIMING_COUNT
#define TIMING_COUNT 1
#endif

#define TRUE 1
#define FALSE 0

unsigned int randomSeed = 1;


static void mmul_serial(int *C, const int *A, const int *B, int n) {
    for (int i = 0; i < n; i++)
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++)
                C[i * n + j] += A[i * n + k] * B[k * n + j];
}

typedef struct {
    int64_t n;
    int *C;
    const int *A, *B;
} outerData;

typedef struct {
    const outerData *o;
    int64_t i, k;
} innerData;

void innerBody(int64_t j, void *data) {
    const innerData *d = data;
    int64_t n = d->o->n;
    int64_t i = d->i, k = d->k;

    d->o->C[i * n + j] += d->o->A[i * n + k] * d->o->B[k * n + j];
}

void outerBody(int64_t i, void *data) {
    const outerData *d = data;
    for (int k = 0; k < d->n; k++) {
        innerData id = {.o=d, .i=i, .k=k};
#ifdef MMUL_PARALLELIZE_INNER
        cilk_for(0, d->n, &id, innerBody, 1);
#else
        for (int j = 0; j < d->n; ++j) {
            innerBody(j, &id);
        }
#endif
    }
}

static void mmul(int *C, const int *A,const int *B, int n) {
    outerData d = {.n=n, .A=A, .B=B, .C=C};
    cilk_for(n, &d, outerBody, 1);
}

static void rand_matrix(int *dest, int n) {
    for (int i = 0; i < n * n; ++i)
        dest[i] = rand_r(&randomSeed) & 0xff;
}

static void zero_matrix(int *dest, int n) {
    for (int i = 0; i < n * n; ++i)
        dest[i] = 0;
}

static int are_equal_matrices(const int *a, const int *b, int n) {
    for (int i = 0; i < n * n; ++i)
        if (a[i] != b[i])
            return FALSE;
    return TRUE;
}

static void test_mmul(int n, int check) {
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    int *A = (int *) malloc(sizeof(int) * (n * n));
    int *B = (int *) malloc(sizeof(int) * (n * n));
    int *C = (int *) malloc(sizeof(int) * (n * n));

    rand_matrix(A, n);
    rand_matrix(B, n);

    for (int i = 0; i < TIMING_COUNT; i++) {
        zero_matrix(C, n);
        begin = ktiming_getmark();
        mmul(C, A, B, n);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    print_runtime(running_time, TIMING_COUNT);

    if (check) {
        fprintf(stderr, "Checking result ...\n");
        int *Cs = (int *) malloc(sizeof(int) * (n * n));
        zero_matrix(Cs, n);
        mmul_serial(Cs, A, B, n);
        if (!are_equal_matrices(C, Cs, n)) {
            fprintf(stderr, "mmul test FAILED.\n");
        } else {
            fprintf(stderr, "mmul test passed.\n");
        }
        free(Cs);
    }

    free(B);
    free(A);
}

// return true iff n = 2^k (or 0).
static int is_power_of_2(int n) {
    return (n & (n - 1)) == 0;
}

const char *specifiers[] = {"-n", "-c", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, BOOLARG, 0};

int main(int argc, char *argv[]) {

    long size;
    int help, check;

    /* standard benchmark options */
    size = 1024;
    check = 0;
    help = 0;

    get_options(argc, argv, specifiers, opt_types, &size, &check, &help);

    if (help) {
        fprintf(stderr, "Usage: mmul_loop [cilk options] -n <size> [-c|-h]\n");
        fprintf(stderr, "   when -c is set, check result against sequential transpose (slow).\n");
        fprintf(stderr, "   when -h is set, print this message and quit.\n");
        exit(0);
    }

    if (!is_power_of_2(size)) {
        fprintf(stderr, "Input size must be a power of 2 \n");
        exit(1);
    }

    test_mmul(size, check);

    return 0;
}
