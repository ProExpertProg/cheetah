//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include <stdint.h>
#include "cilk2c.h"
#include "scheduler.h"

typedef void (*__cilk_abi_f64_t)(void *data, int64_t low, int64_t high);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_helper64(void *data, __cilk_abi_f64_t body, unsigned int grainsize) {
    uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        do {
            body(data, i * grainsize, (i + 1) * grainsize);
            status = __cilkrts_pop_loop_frame(&inner_lf, &i);
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        body(data, i * grainsize, (i + 1) * grainsize);
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    __cilkrts_get_tls_worker()->local_loop_frame = (__cilkrts_loop_frame *) inner_lf.sf.call_parent;

    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
}

void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain) {
    __cilkrts_loop_frame lf;
    uint64_t n = count;
    uint64_t end = n / grain, rem = n % grain;
    __cilkrts_enter_loop_frame(&lf, 0, end);
    // printf("Cilk for\n");

    // cilk_for(int i = low; i < high; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    if (!__builtin_setjmp(lf.sf.ctx)) {
        // first time entering this loop,
        // else is entering the loop after steal
        // make sure the setjmp doesn't get optimized away.

        __cilkrts_get_tls_worker()->local_loop_frame = &lf;

    }

    __cilkrts_cilk_loop_helper64(data, body, grain);

    if (__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if (!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }

    __cilkrts_pop_frame(&local_lf()->sf);
    __cilkrts_leave_loop_frame(local_lf());
    if (rem != 0)
        body(data, count - rem, count);
}

/**
 * 32-bit versions
 */

typedef void (*__cilk_abi_f32_t)(void *data, int32_t low, int32_t high);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_helper32(void *data, __cilk_abi_f32_t body, unsigned int grainsize) {
    uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        do {
            body(data, i * grainsize, (i + 1) * grainsize);
            status = __cilkrts_pop_loop_frame(&inner_lf, &i);
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        body(data, i * grainsize, (i + 1) * grainsize);
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    __cilkrts_get_tls_worker()->local_loop_frame = (__cilkrts_loop_frame *) inner_lf.sf.call_parent;

    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
}

void __cilkrts_cilk_for_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain) {
    __cilkrts_loop_frame lf;
    uint32_t n = count;
    uint32_t end = n / grain, rem = n % grain;
    __cilkrts_enter_loop_frame(&lf, 0, end);
    // printf("Cilk for\n");

    // cilk_for(int i = low; i < high; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    if (!__builtin_setjmp(lf.sf.ctx)) {
        // first time entering this loop,
        // else is entering the loop after steal
        // make sure the setjmp doesn't get optimized away.

        __cilkrts_get_tls_worker()->local_loop_frame = &lf;

    }

    __cilkrts_cilk_loop_helper32(data, body, grain);

    if (__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if (!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }

    __cilkrts_pop_frame(&local_lf()->sf);
    __cilkrts_leave_loop_frame(local_lf());
    if (rem != 0)
        body(data, count - rem, count);
}