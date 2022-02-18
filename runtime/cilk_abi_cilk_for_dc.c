//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include "stdint.h"
#include "cilk_abi_cilk_for.h"
#include "cilk2c_inlined.c"

void cilk_for_root64(uint64_t low, uint64_t high, void *data, __cilk_abi_f64_t body, unsigned int grainsize, int inclusive);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
cilk_loop_helper64(uint64_t low, uint64_t high, void *data, __cilk_abi_f64_t body, unsigned int grainsize, int inclusive) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_helper(&sf);
    __cilkrts_detach(&sf);
    cilk_for_root64(low, high, data, body, grainsize, inclusive);
    __cilk_helper_epilogue(&sf);
}

void cilk_for_root64(uint64_t low, uint64_t high, void *data, __cilk_abi_f64_t body, unsigned int grainsize, int inclusive) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    uint64_t len = high - low;
    while (len > grainsize) {
        uint64_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        if (!__cilk_prepare_spawn(&sf)) {
            cilk_loop_helper64(low, mid, data, body, grainsize, inclusive);
        }

        low = mid + inclusive;
        len = high - low;
    }

    body(data, low, high);

    /* cilk_sync */
    __cilk_sync_nothrow(&sf);
    __cilk_parent_epilogue(&sf);
}

static void cilk_for_impl_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain, int inclusive) {
    cilk_for_root64(0, count, data, body, grain, inclusive);
}

/**
 * 32-bit versions
 */

void cilk_for_root32(uint32_t low, uint32_t high, void *data, __cilk_abi_f32_t body, unsigned grainsize,
                     int inclusive);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
cilk_loop_helper32(uint32_t low, uint32_t high, void *data, __cilk_abi_f32_t body, unsigned int grainsize,
                   int inclusive) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_helper(&sf);
    __cilkrts_detach(&sf);
    cilk_for_root32(low, high, data, body, grainsize, inclusive);
    __cilk_helper_epilogue(&sf);
}

void cilk_for_root32(uint32_t low, uint32_t high, void *data, __cilk_abi_f32_t body, unsigned grainsize,
                     int inclusive) {

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    uint32_t len = high - low;
    while (len > grainsize) {
        uint32_t mid = low + len / 2;

        // cilk_spawn cilk_loop_helper()
        if (!__cilk_prepare_spawn(&sf)) {
            cilk_loop_helper32(low, mid, data, body, grainsize, inclusive);
        }

        low = mid + inclusive;
        len = high - low;
    }

    body(data, low, high);

    /* cilk_sync */
    __cilk_sync_nothrow(&sf);

    __cilk_parent_epilogue(&sf);
}

static void cilk_for_impl_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain, int inclusive) {
    cilk_for_root32(0, count, data, body, grain, inclusive);
}