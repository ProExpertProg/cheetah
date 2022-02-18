#ifndef TIMING_COUNT
#define TIMING_COUNT 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <string.h>

#include "cilk_for.h"

extern size_t ZERO;
void __attribute__((weak)) dummy(void *p) { return; }

int ok(int n, char *a);

int nqueens(int n, int j, const char *a);

typedef struct {
    int n, j;
    const char *a;

    int *count; // return value
} forData;

static void body(uint64_t i, void *data) {

    const forData *d = data;
    /***
     * Strictly speaking, this (alloca after spawn) is frowned
     * up on, but in this case, this is ok, because b returned by
     * alloca is only used in this iteration; later spawns don't
     * need to be able to access copies of b from previous iterations
     ***/
    char *b = (char *) alloca((d->j + 1) * sizeof(char));
    memcpy(b, d->a, d->j * sizeof(char));
    b[d->j] = i;

    if (ok(d->j + 1, b)) {
        d->count[i] = nqueens(d->n, d->j + 1, b);
    }
}

int nqueens(int n, int j, const char *a) {

    dummy(alloca(ZERO));
    char *b;
    int i;
    int *count;
    int solNum = 0;

    if (n == j) {
        return 1;
    }

    count = (int *) alloca(n * sizeof(int));
    (void) memset(count, 0, n * sizeof(int));

    dummy(alloca(ZERO));

    forData data = {.count=count, .j=j, .a=a, .n=n};
    cilk_for(n, &data, body, 1);

    for (i = 0; i < n; i++) {
        solNum += count[i];
    }

    return solNum;
}