// Checks how __cilkrts_cilk_for_{64,32} chunk a loop.
//
//   cilk_for_chunks [--spread]
//
// 1. Pure arithmetic (no runtime): for all n, G in a range, the "spread the remainder" chunking
//        m = n / G, r = n % G, G2 = G + r / m, r2 = r % m,  chunk i = [i*G2 + min(i, r2), i*G2 + G2 + min(i+1, r2))
//    partitions [0, n) into m contiguous chunks of G2 + 1 (first r2) or G2 iterations, every chunk has at least G and
//    less than 2G iterations, and it is the original chunking when r == 0.
// 2. Runtime: every iteration runs exactly once, and (inclusive == 0) the chunks are the original ones
//    (m chunks of G plus one of r) or, with --spread, the spread ones above (m chunks when r != 0 and m >= 1).
//    Inclusive loops must keep the original schedule in both modes.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"

typedef struct {
    uint64_t lo, hi;
} range_t;

#define MAX_CALLS (1u << 20)
static range_t calls[MAX_CALLS];
static uint64_t ncalls;

static void record64(void *d, uint64_t lo, uint64_t hi) {
    (void)d;
    // a little work so that thieves get a chance to steal
    for (volatile int k = 0; k < 200; k++)
        ;
    uint64_t at = __atomic_fetch_add(&ncalls, 1, __ATOMIC_RELAXED);
    if (at < MAX_CALLS) {
        calls[at].lo = lo;
        calls[at].hi = hi;
    }
}

static void record32(void *d, uint32_t lo, uint32_t hi) { record64(d, lo, hi); }

static int cmp_range(const void *a, const void *b) {
    const range_t *x = a, *y = b;
    return x->lo < y->lo ? -1 : x->lo > y->lo;
}

static int failures;
#define FAIL(...)                                                     \
    do {                                                              \
        if (failures++ < 30) {                                        \
            printf("FAIL: ");                                         \
            printf(__VA_ARGS__);                                      \
            printf("\n");                                             \
        }                                                             \
    } while (0)

static uint64_t umin(uint64_t a, uint64_t b) { return a < b ? a : b; }

// ---- 1. pure arithmetic ----
static void check_arithmetic(void) {
    uint64_t cases = 0;
    for (uint64_t n = 0; n <= 3000; n++) {
        for (uint64_t G = 1; G <= 80; G++) {
            uint64_t m = n / G, r = n % G;
            if (m == 0 || r == 0)
                continue; // original chunking is used
            uint64_t G2 = G + r / m, r2 = r % m, prev_hi = 0;
            for (uint64_t i = 0; i < m; i++) {
                uint64_t lo = i * G2 + umin(i, r2);
                uint64_t hi = i * G2 + G2 + umin(i + 1, r2);
                uint64_t size = hi - lo;
                if (lo != prev_hi) FAIL("gap/overlap n=%lu G=%lu chunk %lu", n, G, i);
                if (size != G2 + (i < r2)) FAIL("size n=%lu G=%lu chunk %lu", n, G, i);
                if (size < G || size >= 2 * G) FAIL("size bound n=%lu G=%lu chunk %lu size %lu", n, G, i, size);
                prev_hi = hi;
            }
            if (prev_hi != n) FAIL("does not cover n=%lu G=%lu (ends at %lu)", n, G, prev_hi);
            cases++;
        }
    }
    printf("arithmetic: %lu (n, G) pairs with m >= 1 and r != 0 checked\n", cases);
}

// ---- 2. runtime ----
static void expected_chunks(uint64_t n, uint64_t G, int spread, range_t *out, uint64_t *cnt) {
    uint64_t m = n / G, r = n % G, k = 0;
    if (spread && m >= 1 && r != 0) {
        uint64_t G2 = G + r / m, r2 = r % m;
        for (uint64_t i = 0; i < m; i++) {
            out[k].lo = i * G2 + umin(i, r2);
            out[k].hi = i * G2 + G2 + umin(i + 1, r2);
            k++;
        }
    } else {
        for (uint64_t i = 0; i < m; i++) {
            out[k].lo = i * G;
            out[k].hi = i * G + G;
            k++;
        }
        if (r != 0) {
            out[k].lo = m * G;
            out[k].hi = n;
            k++;
        }
    }
    *cnt = k;
}

static range_t expect[MAX_CALLS];

static void check_one(uint64_t n, unsigned G, int bits, int inclusive, int spread) {
    ncalls = 0;
    if (bits == 64) {
        if (inclusive) __cilkrts_cilk_for_inclusive_64(record64, NULL, n, G);
        else __cilkrts_cilk_for_64(record64, NULL, n, G);
    } else {
        if (inclusive) __cilkrts_cilk_for_inclusive_32(record32, NULL, (uint32_t)n, G);
        else __cilkrts_cilk_for_32(record32, NULL, (uint32_t)n, G);
    }
    uint64_t got = ncalls;
    if (got > MAX_CALLS) {
        FAIL("too many chunks n=%lu G=%u", n, G);
        return;
    }
    qsort(calls, got, sizeof(range_t), cmp_range);

    // every iteration exactly once: contiguous, starting at 0, ending at n (exclusive) or n inclusive
    uint64_t next = 0;
    for (uint64_t i = 0; i < got; i++) {
        uint64_t lo = calls[i].lo, hi = calls[i].hi + (inclusive ? 1 : 0); // inclusive bodies get [lo, hi]
        if (lo != next) {
            FAIL("bits=%d incl=%d n=%lu G=%u: chunk %lu starts at %lu, expected %lu", bits, inclusive, n, G, i, lo, next);
            return;
        }
        next = hi;
    }
    uint64_t want_total = n + (inclusive ? 1 : 0);
    if (next != want_total) {
        FAIL("bits=%d incl=%d n=%lu G=%u: covers [0,%lu), expected [0,%lu)", bits, inclusive, n, G, next, want_total);
        return;
    }

    if (!inclusive) {
        uint64_t ecnt;
        expected_chunks(n, G, spread, expect, &ecnt);
        if (got != ecnt) {
            FAIL("bits=%d n=%lu G=%u spread=%d: %lu chunks, expected %lu", bits, n, G, spread, got, ecnt);
            return;
        }
        for (uint64_t i = 0; i < ecnt; i++)
            if (calls[i].lo != expect[i].lo || calls[i].hi != expect[i].hi) {
                FAIL("bits=%d n=%lu G=%u spread=%d: chunk %lu is [%lu,%lu), expected [%lu,%lu)", bits, n, G, spread, i,
                     calls[i].lo, calls[i].hi, expect[i].lo, expect[i].hi);
                return;
            }
    }
}

int main(int argc, char **argv) {
    int spread = argc > 1 && strcmp(argv[1], "--spread") == 0;
    check_arithmetic();

    static const uint64_t ns[] = {0, 1, 2, 3, 5, 12, 13, 24, 25, 100, 127, 128, 129, 255, 256, 257, 1000,
                                  4095, 4096, 4097, 5001, 12345, 100003};
    static const unsigned Gs[] = {1, 2, 3, 4, 5, 7, 12, 13, 16, 100, 127, 128, 2048};
    uint64_t checks = 0;
    for (size_t a = 0; a < sizeof ns / sizeof *ns; a++)
        for (size_t b = 0; b < sizeof Gs / sizeof *Gs; b++)
            for (int bits = 32; bits <= 64; bits += 32)
                for (int inclusive = 0; inclusive <= 1; inclusive++) {
                    check_one(ns[a], Gs[b], bits, inclusive, spread);
                    checks++;
                }
    printf("runtime: %lu loops checked (schedule: %s)\n", checks, spread ? "spread remainder" : "original");
    printf(failures ? "FAILED (%d)\n" : "PASSED\n", failures);
    return failures != 0;
}
