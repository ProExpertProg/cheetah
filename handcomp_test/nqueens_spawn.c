#ifndef TIMING_COUNT
#define TIMING_COUNT 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/cilk2c.h"
#include "../runtime/cilk2c_inlined.c"

extern size_t ZERO;
void __attribute__((weak)) dummy(void *p) { return; }

int ok (int n, char *a);

static void __attribute__ ((noinline))
nqueens_spawn_helper(int *count, int n, int j, char *a);

int nqueens(int n, int j, char *a) {

    char *b;
    int i;
    int *count;
    int solNum = 0;

    if (n == j) {
        return 1;
    }

    count = (int *) alloca(n * sizeof(int));
    (void) memset(count, 0, n * sizeof (int));

    dummy(alloca(ZERO));
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    for (i = 0; i < n; i++) {

        /***
         * Strictly speaking, this (alloca after spawn) is frowned
         * up on, but in this case, this is ok, because b returned by
         * alloca is only used in this iteration; later spawns don't
         * need to be able to access copies of b from previous iterations
         ***/
        b = (char *) alloca((j + 1) * sizeof (char));
        memcpy(b, a, j * sizeof (char));
        b[j] = i;

        if(ok (j + 1, b)) {

            /* count[i] = cilk_spawn nqueens(n, j + 1, b); */
            __cilkrts_save_fp_ctrl_state(&sf);
            if(!__builtin_setjmp(sf.ctx)) {
                nqueens_spawn_helper(&(count[i]), n, j+1, b);
            }
        }
    }
    /* cilk_sync */
    if(sf.flags & CILK_FRAME_UNSYNCHED) {
      __cilkrts_save_fp_ctrl_state(&sf);
      if(!__builtin_setjmp(sf.ctx)) {
        __cilkrts_sync(&sf);
      }
    }

    for(i = 0; i < n; i++) {
        solNum += count[i];
    }

    __cilkrts_pop_frame(&sf);
    if (0 != sf.flags)
        __cilkrts_leave_frame(&sf);

    return solNum;
}

static void __attribute__ ((noinline))
nqueens_spawn_helper(int *count, int n, int j, char *a) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);
    __cilkrts_detach(&sf);
    *count = nqueens(n, j, a);
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}