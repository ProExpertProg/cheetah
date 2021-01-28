//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include "stdint.h"
#include "cilk2c.h"
#include "scheduler.h"

typedef void (*__cilk_abi_f64_t)(void *data, int64_t low, int64_t high);

void cilk_for_root(uint64_t low, uint64_t high, void *data, __cilk_abi_f64_t body, uint64_t grainsize);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
cilk_loop_helper(uint64_t low, uint64_t high, void *data, __cilk_abi_f64_t body, uint64_t grainsize) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast(&sf);
    __cilkrts_detach(&sf);
    cilk_for_root(low, high, data, body, grainsize);
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

void cilk_for_root(uint64_t low, uint64_t high, void *data, __cilk_abi_f64_t body, uint64_t grainsize) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);


    uint64_t len = high - low;
    while (len > grainsize) {
        uint64_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        __cilkrts_save_fp_ctrl_state(&sf);
        if (!__builtin_setjmp(sf.ctx)) {
            cilk_loop_helper(low, mid, data, body, grainsize);
        }

        low = mid;
        len = high - low;
    }

    body(data, low, high);

    /* cilk_sync */
    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        __cilkrts_save_fp_ctrl_state(&sf);
        if (!__builtin_setjmp(sf.ctx)) {
            __cilkrts_sync(&sf);
        }
    }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

}

void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, uint64_t count, int grain) {
    cilk_for_root(0, count, data, body, grain);
}