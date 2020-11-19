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


static void m_transpose_serial(int *B, const int *A, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = i; j < n; j++) {
            B[j * n + i] = A[i * n + j];
        }
    }
}

typedef struct {
    uint n;
    int *B;
    const int *A;
} outerData;

typedef struct {
    const outerData *o;
    uint64_t i;
} innerData;

void innerBody(uint64_t j, void *data) {
    const innerData *d = data;
    d->o->B[j * d->o->n + d->i] = d->o->A[d->i * d->o->n + j];
}

void outerBody(uint64_t i, void *data) {
    const outerData *d = data;
    innerData id = {.o=d, .i=i};
#ifdef MTRANS_PARALLELIZE_INNER
    cilk_for(i, d->n, &id, innerBody, 1);
#else
    for (int j = i; j < d->n; ++j) {
        innerBody(j, &id);
    }
#endif
}

static void m_transpose(int *B, const int *A, int n) {
    outerData d = {.n=n, .A=A, .B=B};
    cilk_for(0, n, &d, outerBody, 1);
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
    for(int i = 0; i < n*n; ++i)
        if(a[i] != b[i])
            return FALSE;
    return TRUE;
}

static void test_mtrans(int n, int check) {
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    int *A = (int *) malloc(sizeof(int) * (n * n));
    int *B = (int *) malloc(sizeof(int) * (n * n));

    rand_matrix(A, n);

    for (int i = 0; i < TIMING_COUNT; i++) {
        zero_matrix(B, n);
        begin = ktiming_getmark();
        m_transpose(B, A, n);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);
    }
    print_runtime(running_time, TIMING_COUNT);

    if (check) {
        fprintf(stderr, "Checking result ...\n");
        int *Bs = (int *) malloc(sizeof(int) * (n * n));
        zero_matrix(Bs, n);
        m_transpose_serial(Bs, A, n);
        if (!are_equal_matrices(B, Bs, n)) {
            fprintf(stderr, "mtrans test FAILED.\n");
        } else {
            fprintf(stderr, "mtrans test passed.\n");
        }
        free(Bs);
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

int cilk_main(int argc, char *argv[]) {

    long size;
    int help, check;

    /* standard benchmark options */
    size = 1024;
    check = 0;
    help = 0;

    get_options(argc, argv, specifiers, opt_types, &size, &check, &help);

    if (help) {
        fprintf(stderr, "Usage: mtrans [cilk options] -n <size> [-c|-h]\n");
        fprintf(stderr, "   when -c is set, check result against sequential transpose (slow).\n");
        fprintf(stderr, "   when -h is set, print this message and quit.\n");
        exit(0);
    }

    if (!is_power_of_2(size)) {
        fprintf(stderr, "Input size must be a power of 2 \n");
        exit(1);
    }

    test_mtrans(size, check);

    return 0;
}
