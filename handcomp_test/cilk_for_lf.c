//
// Created by Luka on 11/10/2020.
//

#include "cilk_for.h"
#include "../runtime/cilk2c.h"
#include "../runtime/scheduler.h"

// we cannot inline this function because of local variables
static void __attribute__ ((noinline)) cilk_loop_helper(void *data, ForBody body, uint64_t low, uint64_t grainsize) {
    uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        do {
            for (uint64_t j = i * grainsize + low; j < (i + 1) * grainsize + low; j++) {
                body(j, data);
            }
            status = __cilkrts_pop_loop_frame(&inner_lf, &i);
        } while (status == SUCCESS_ITERATION);
    }

    if(status == SUCCESS_LAST_ITERATION) {
        for (uint64_t j = i * grainsize + low; j < (i + 1) * grainsize + low; j++) {
            body(j, data);
        }
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    __cilkrts_get_tls_worker()->local_loop_frame = (__cilkrts_loop_frame *) inner_lf.sf.call_parent;
    CILK_ASSERT(__cilkrts_get_tls_worker(), local_lf() == inner_lf.parentLF);

    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
}


void cilk_for(uint64_t low, uint64_t high, void *data, ForBody body, uint64_t grainsize) {

    __cilkrts_loop_frame lf;
    uint64_t n = high - low;
    uint64_t end = n / grainsize, rem = n % grainsize;
    __cilkrts_enter_loop_frame(&lf, 0, end);

    // cilk_for(int i = low; i < high; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    if (!__builtin_setjmp(lf.sf.ctx)) {
        // first time entering this loop,
        // else is entering the loop after steal
        // make sure the setjmp doesn't get optimized away.

        __cilkrts_get_tls_worker()->local_loop_frame = &lf;

    }

    cilk_loop_helper(data, body, low, grainsize);

    WHEN_CILK_DEBUG(__cilkrts_worker * w = __cilkrts_get_tls_worker());
    CILK_ASSERT(w, local_lf()->start == local_lf()->end);

    if (__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if (!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }

    __cilkrts_pop_frame(&local_lf()->sf);
    __cilkrts_leave_loop_frame(local_lf());

    for (uint64_t i = high - rem; i < high; ++i) {
        body(i, data);
    }

}