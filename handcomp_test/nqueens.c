#ifndef TIMING_COUNT
#define TIMING_COUNT 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"
#include "../runtime/cilk2c_inlined.c"
#include "ktiming.h"

extern size_t ZERO;
void __attribute__((weak)) dummy(void *p) { return; }

// int * count;

/* 
 * nqueen  4 = 2 
 * nqueen  5 = 10 
 * nqueen  6 = 4 
 * nqueen  7 = 40 
 * nqueen  8 = 92 
 * nqueen  9 = 352 
 * nqueen 10 = 724
 * nqueen 11 = 2680 
 * nqueen 12 = 14200 
 * nqueen 13 = 73712 
 * nqueen 14 = 365596 
 * nqueen 15 = 2279184 
 */

/*
 * <a> contains array of <n> queen positions.  Returns 1
 * if none of the queens conflict, and returns 0 otherwise.
 */
static int ok (int n, char *a) {

    int i, j;
    char p, q;

    for (i = 0; i < n; i++) {
        p = a[i];
        for (j = i + 1; j < n; j++) {
            q = a[j];
            if (q == p || q == p - (j - i) || q == p + (j - i))
                return 0;
        }
    }

    return 1;
}

// at link time, either the one using spawns or the one using cilk_for
// for cilk_for, we also choose divide-and-conquer or loop frames
int nqueens(int n, int j, const char *a);

int main(int argc, char *argv[]) {
  int n = 13;
  char *a;
  int res;

  if(argc < 2) {
      fprintf (stderr, "Usage: %s <n>\n", argv[0]);
      fprintf (stderr, "Use default board size, n = 13.\n");
      exit(0);
  } else {
      n = atoi (argv[1]);
      printf ("Running %s with n = %d.\n", argv[0], n);
  }

  a = (char *) alloca (n * sizeof (char));
  res = 0;

#if TIMING_COUNT
  clockmark_t begin, end;
  uint64_t elapsed[TIMING_COUNT];

  for(int i=0; i < TIMING_COUNT; i++) {
      begin = ktiming_getmark();
      res = nqueens(n, 0, a);
      end = ktiming_getmark();
      elapsed[i] = ktiming_diff_nsec(&begin, &end);
  }
  print_runtime(elapsed, TIMING_COUNT);
#else
  res = nqueens(n, 0, a);
#endif

  if (res == 0) {
      printf("No solution found.\n");
  } else {
      printf("Total number of solutions : %d\n", res);
  }

  return 0;
}
