//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include "stdint.h"
#include "cilk2c.h"
#include "cilk2c_inlined.c"

void cilk_for_root64(int64_t low, int64_t high, void *data, __cilk_abi_f64_t body, unsigned int grainsize);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
cilk_loop_helper64(int64_t low, int64_t high, void *data, __cilk_abi_f64_t body, unsigned int grainsize) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_helper(&sf);
    __cilkrts_detach(&sf);
    cilk_for_root64(low, high, data, body, grainsize);
    __cilk_helper_epilogue(&sf);
}

void cilk_for_root64(int64_t low, int64_t high, void *data, __cilk_abi_f64_t body, unsigned int grainsize) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    int64_t len = high - low;
    while (len > grainsize) {
        int64_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        if (!__cilk_prepare_spawn(&sf)) {
            cilk_loop_helper64(low, mid, data, body, grainsize);
        }

        low = mid;
        len = high - low;
    }

    body(data, low, high);

    /* cilk_sync */
    __cilk_sync_nothrow(&sf);
    __cilk_parent_epilogue(&sf);
}

void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, int64_t count, unsigned int grain) {
    cilk_for_root64(0, count, data, body, grain);
}

/**
 * 32-bit versions
 */

void cilk_for_root32(int32_t low, int32_t high, void *data, __cilk_abi_f32_t body, unsigned int grainsize);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
cilk_loop_helper32(int32_t low, int32_t high, void *data, __cilk_abi_f32_t body, unsigned int grainsize) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_helper(&sf);
    __cilkrts_detach(&sf);
    cilk_for_root32(low, high, data, body, grainsize);
    __cilk_helper_epilogue(&sf);
}

void cilk_for_root32(int32_t low, int32_t high, void *data, __cilk_abi_f32_t body, unsigned grainsize) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    int32_t len = high - low;
    while (len > grainsize) {
        int32_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        if (!__cilk_prepare_spawn(&sf)) {
            cilk_loop_helper32(low, mid, data, body, grainsize);
        }

        low = mid;
        len = high - low;
    }

    body(data, low, high);

    /* cilk_sync */
    __cilk_sync_nothrow(&sf);

    __cilk_parent_epilogue(&sf);
}

void __cilkrts_cilk_for_32(__cilk_abi_f32_t body, void *data, int32_t count, unsigned int grain) {
    cilk_for_root32(0, count, data, body, grain);
}