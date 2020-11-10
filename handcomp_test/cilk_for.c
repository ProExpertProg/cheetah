//
// Created by Luka on 11/10/2020.
//

#include "cilk_for.h"
#include "../runtime/cilk2c.h"

// we cannot inline this function because of local variables
static void __attribute__ ((noinline)) cilk_loop_helper(uint64_t low, uint64_t high, void *data, ForBody body, uint64_t grainsize) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);
    __cilkrts_detach(&sf);
    cilk_for(low, high, data, body, grainsize);
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}


void cilk_for(uint64_t low, uint64_t high, void *data, ForBody body, uint64_t grainsize) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);


    uint64_t len = high - low;
    while (len > grainsize) {
        uint64_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        __cilkrts_save_fp_ctrl_state(&sf);
        if(!__builtin_setjmp(sf.ctx)) {
            cilk_loop_helper(low, mid, data, body, grainsize);
        }

        low = mid;
        len = high - low;
    }

    for (int i = low; i < high; ++i) {
        // body
        body(i, data);
    }

    /* cilk_sync */
    if(sf.flags & CILK_FRAME_UNSYNCHED) {
        __cilkrts_save_fp_ctrl_state(&sf);
        if(!__builtin_setjmp(sf.ctx)) {
            __cilkrts_sync(&sf);
        }
    }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

}