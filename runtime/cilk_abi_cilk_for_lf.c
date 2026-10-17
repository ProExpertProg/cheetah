//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include <stdint.h>
#include "cilk_abi_cilk_for.h"
#include "cilk2c_inlined.c"
#include "scheduler.h"
#include "readydeque.h"
#include "local.h"

#if INLINE_POP_LF
#define ATTR_POP_LF __always_inline
#else
#define ATTR_POP_LF __attribute_noinline__
#endif

ATTR_POP_LF __cilkrts_iteration_return __cilkrts_loop_frame_next(__cilkrts_inner_loop_frame *lf, __cilkrts_loop_frame *pLoopFrame) {
    __cilkrts_worker *w = get_worker_from_stack(&lf->sf);
//    cilkrts_alert(CFRAME,
//                    "(__cilkrts_loop_frame_next) attempting to obtain another iteration for frame %p",
//                    lf);
    CILK_ASSERT(CHECK_CILK_FRAME_MAGIC(w->g, &lf->sf));
    CILK_ASSERT(w == __cilkrts_get_tls_worker());
    CILK_ASSERT(&pLoopFrame->sf == lf->sf.fh->current_stack_frame);

    CILK_ASSERT(lf->sf.flags & CILK_FRAME_DETACHED);
    CILK_ASSERT(__cilkrts_is_inner_loop(&lf->sf));
    CILK_ASSERT(__cilkrts_is_loop(&pLoopFrame->sf));

    // THESE protocol

    CILK_ASSERT(lf->parentLF == pLoopFrame);

    // safe to read tail as we're the only ones updating it
    // either:
    // - we're at the boundary of the stack (we stole something nested and are now walking back up,
    //   with access to the loop frame as but not on our deque (not ever)
    // - the loop frame is on our deque, whether we have access to it or not
    CILK_ASSERT(w->tail == w->l->shadow_stack || *(w->tail - 1) == &pLoopFrame->sf);

    // we just need to make sure that the load of end happens after store of start
    uint64_t start = __atomic_load_n(&pLoopFrame->start, __ATOMIC_RELAXED);
    start++;
    __atomic_store_n(&pLoopFrame->start, start, __ATOMIC_SEQ_CST);

    if (__builtin_expect(start > __atomic_load_n(&pLoopFrame->end, __ATOMIC_SEQ_CST), 0)) {
        ReadyDeque *deques = w->g->deques;
        worker_id self = w->self;
        deque_lock_self(deques, self);
        // no need for a fence because we now have exclusive access
        // also, the lock already fenced (it should have??)
        if (start > __atomic_load_n(&pLoopFrame->end, __ATOMIC_SEQ_CST)) {
            pLoopFrame->start--;
            deque_unlock_self(deques, self);
            return FAIL_ITERATION;
        }
        deque_unlock_self(deques, self);
    }

    // TODO perhaps the optimization if LF is left empty

    return SUCCESS_ITERATION;
}

static __attribute__((noinline)) __cilkrts_loop_frame *current_loop_frame(void) {
    return (__cilkrts_loop_frame *)__cilkrts_current_fh->current_stack_frame;
}

/* We handle both types of loops with the same function.
 * - If exclusive, we execute [0, count) in chunks of [i, i + grainsize), always incrementing i by grainsize.
 *   We execute a remainder chunk if any iterations are left.
 * - If inclusive, we first execute [0, count) like above by executing chunks [i, i + grainsize - 1].
 *   After that, we're always left with a remainder (even if count is perfectly divisible by grainsize)
 *   Worst case, the remainder chunk could have been executed by the last iteration as it might have grainsize iterations,
 *   but we avoid integer overflow issues that would arise if we artificially incremented count by 1 in the beginning.
 */

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_helper64(void *data, __cilk_abi_f64_t body, unsigned int grainsize, int inclusive,
                          __cilkrts_loop_frame *parent) {
    FORCE_VAR_SIZED_FRAME();
    uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf, parent);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, parent, &i);
    i *= grainsize;
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf, &parent->sf); // push the parent loop_frame to the deque

        do {
            CILK_ASSERT(i + grainsize == parent->start * grainsize);
            body(data, i, i + grainsize - inclusive);
            status = __cilkrts_loop_frame_next(&inner_lf, parent);
            i += grainsize;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        body(data, i, i + grainsize - inclusive);
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    get_worker_from_stack(&inner_lf.sf)->local_loop_frame = parent;
    CILK_ASSERT(local_lf() == parent);

    // The inner loop frame is not a spawner, so if it was never detached there
    // is nothing to pop.
    if (inner_lf.sf.flags & CILK_FRAME_DETACHED)
        __cilk_helper_epilogue(&inner_lf.sf, &parent->sf, /* spawner */ false);
}

static void cilk_for_loop_64(__cilk_abi_f64_t body, void *data, uint64_t end, unsigned int grain, int inclusive) {
    FORCE_VAR_SIZED_FRAME();
    __cilkrts_loop_frame lf;
    __cilkrts_enter_loop_frame(&lf, 0, end);

    // cilk_for(int i = low; i < high; ++i) {

    // Contains the setjmp. We continue to the loop helper in both setjmp cases.
    __cilk_prepare_spawn(&lf.sf);

    __cilkrts_cilk_loop_helper64(data, body, grain, inclusive, current_loop_frame());

    CILK_ASSERT(local_lf()->start == local_lf()->end);

    // cannot use __cilkrts_sync because we cannot allow local_lf to be stored in a variable on the stack
    // maybe there's a way of doing that? still need setjmp in this function though
    if (!__cilkrts_synced(&local_lf()->sf)) {
        if (__builtin_setjmp(local_lf()->sf.ctx) == 0) {
            sysdep_save_fp_ctrl_state(&local_lf()->sf);
            __cilkrts_sync(&local_lf()->sf);
        } else {
            sanitizer_finish_switch_fiber();
        }
    }

    __cilkrts_leave_loop_frame(local_lf());
    // after the last frame we have left the cilkified region
    CILK_ASSERT(__cilkrts_need_to_cilkify || local_lf() == &lf);
}

__attribute__((noinline))
static void cilk_for_loop_helper_64(__cilk_abi_f64_t body, void *data, uint64_t end, unsigned int grain, int inclusive,
                                         __cilkrts_stack_frame *parent) {
    __cilkrts_stack_frame sf;
    // the helper only calls cilk_for_loop, which sets up its own frame
    __cilkrts_enter_frame_helper(&sf, parent, /* spawner */ false);
    __cilkrts_detach(&sf, parent);
    cilk_for_loop_64(body, data, end, grain, inclusive);
    __cilk_helper_epilogue(&sf, parent, /* spawner */ false);
}

/* Spreading the remainder.
 *
 * A loop of n iterations with grainsize G has m = n / G full chunks and r = n % G leftover iterations. The
 * original schedule runs m chunks of G iterations in the loop frame and the r leftover iterations as one extra
 * chunk, i.e. m + 1 chunks, the last of them short. When r != 0 and m >= 1 we instead run exactly m chunks and
 * give every chunk a share of the leftover iterations:
 *     G2 = G + r / m,   r2 = r % m
 *     chunk i (0 <= i < m) is [lo(i), hi(i)) with lo(i) = i * G2 + min(i, r2),
 *                                                  hi(i) = lo(i) + G2 + (i < r2)
 * so the first r2 chunks have G2 + 1 iterations and the others G2. The chunks are contiguous, in order, cover
 * [0, n) exactly once (hi(i) == lo(i + 1), lo(0) == 0, hi(m - 1) == m * G2 + r2 == n), and each has at least G and
 * less than 2G iterations. When r == 0 this is exactly the original schedule. Inclusive loops and loops with
 * m == 0 keep the original schedule. The loop frame only counts chunk indices, so splitting by thieves is unchanged.
 */

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_spread_helper64(void *data, __cilk_abi_f64_t body, uint64_t g2, uint64_t r2,
                                    __cilkrts_loop_frame *parent) {
    FORCE_VAR_SIZED_FRAME();
    uint64_t i; // chunk index
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf, parent);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, parent, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf, &parent->sf); // push the parent loop_frame to the deque

        do {
            CILK_ASSERT(i + 1 == parent->start);
            uint64_t lo = i * g2 + (i < r2 ? i : r2);
            body(data, lo, lo + g2 + (i < r2));
            status = __cilkrts_loop_frame_next(&inner_lf, parent);
            i++;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        uint64_t lo = i * g2 + (i < r2 ? i : r2);
        body(data, lo, lo + g2 + (i < r2));
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    get_worker_from_stack(&inner_lf.sf)->local_loop_frame = parent;
    CILK_ASSERT(local_lf() == parent);

    // The inner loop frame is not a spawner, so if it was never detached there
    // is nothing to pop.
    if (inner_lf.sf.flags & CILK_FRAME_DETACHED)
        __cilk_helper_epilogue(&inner_lf.sf, &parent->sf, /* spawner */ false);
}

static void cilk_for_loop_spread_64(__cilk_abi_f64_t body, void *data, uint64_t end, uint64_t g2, uint64_t r2) {
    FORCE_VAR_SIZED_FRAME();
    __cilkrts_loop_frame lf;
    __cilkrts_enter_loop_frame(&lf, 0, end);

    // Contains the setjmp. We continue to the loop helper in both setjmp cases.
    __cilk_prepare_spawn(&lf.sf);

    __cilkrts_cilk_loop_spread_helper64(data, body, g2, r2, current_loop_frame());

    CILK_ASSERT(local_lf()->start == local_lf()->end);

    if (!__cilkrts_synced(&local_lf()->sf)) {
        if (__builtin_setjmp(local_lf()->sf.ctx) == 0) {
            sysdep_save_fp_ctrl_state(&local_lf()->sf);
            __cilkrts_sync(&local_lf()->sf);
        } else {
            sanitizer_finish_switch_fiber();
        }
    }

    __cilkrts_leave_loop_frame(local_lf());
    // after the last frame we have left the cilkified region
    CILK_ASSERT(__cilkrts_need_to_cilkify || local_lf() == &lf);
}

static void cilk_for_impl_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain, int inclusive) {
    uint64_t end, rem;

    if (grain == 1) {
        end = count;
        rem = 0;
    } else if (((grain - 1) & grain) == 0) { // power of 2 grainsize
        end = count >> __builtin_ctz(grain); // trailing zeroes intrinsic
        rem = count & (grain - 1);
    } else {
        end = count / grain, rem = count % grain;
    }

    // sanity check
    CILK_ASSERT(end == count / grain);
    CILK_ASSERT(rem == count % grain);

    // spread the remainder over the chunks (see "Spreading the remainder")
    if (rem != 0 && end != 0 && !inclusive) {
        uint64_t g2 = (uint64_t)grain + rem / end, r2 = rem % end;
        return cilk_for_loop_spread_64(body, data, end, g2, r2);
    }

    // if inclusive, remainder always contains at least an iteration
    if (rem == 0 && !inclusive) {
        // no remainder to spawn
        return cilk_for_loop_64(body, data, end, grain, inclusive);
    }

    // spawn the body, continue with the remainder
    FORCE_VAR_SIZED_FRAME();
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    // cilk_spawn cilk_for_loop_64(body, data, end, grain, inclusive);
    if(!__cilk_prepare_spawn(&sf)) {
        cilk_for_loop_helper_64(body, data, end, grain, inclusive, &sf);
    }

    // remainder is the continuation
    body(data, count - rem, count);

    // cilk_sync
    __cilk_sync_nothrow(&sf);
    __cilk_parent_epilogue(&sf);
}

/**
 * 32-bit versions
 */

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_helper32(void *data, __cilk_abi_f32_t body, unsigned int grainsize, int inclusive,
                          __cilkrts_loop_frame *parent) {
    FORCE_VAR_SIZED_FRAME();
    uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf, parent);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, parent, &i);
    i *= grainsize;
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf, &parent->sf); // push the parent loop_frame to the deque

        do {
            CILK_ASSERT(i + grainsize == parent->start * grainsize);
            body(data, i, i + grainsize - inclusive);
            status = __cilkrts_loop_frame_next(&inner_lf, parent);
            i += grainsize;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        body(data, i, i + grainsize - inclusive);
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    get_worker_from_stack(&inner_lf.sf)->local_loop_frame = parent;
    CILK_ASSERT(local_lf() == parent);

    // The inner loop frame is not a spawner, so if it was never detached there
    // is nothing to pop.
    if (inner_lf.sf.flags & CILK_FRAME_DETACHED)
        __cilk_helper_epilogue(&inner_lf.sf, &parent->sf, /* spawner */ false);
}

static void cilk_for_loop_32(__cilk_abi_f32_t body, void *data, uint32_t end, unsigned int grain, int inclusive) {
    FORCE_VAR_SIZED_FRAME();
    __cilkrts_loop_frame lf;
    __cilkrts_enter_loop_frame(&lf, 0, end);

    // cilk_for(int i = low; i < high; ++i) {

    // Contains the setjmp. We continue to the loop helper in both setjmp cases.
    __cilk_prepare_spawn(&lf.sf);

    __cilkrts_cilk_loop_helper32(data, body, grain, inclusive, current_loop_frame());

    CILK_ASSERT(local_lf()->start == local_lf()->end);

    // cannot use __cilkrts_sync because we cannot allow local_lf to be stored in a variable on the stack
    // maybe there's a way of doing that? still need setjmp in this function though
    if (!__cilkrts_synced(&local_lf()->sf)) {
        if (__builtin_setjmp(local_lf()->sf.ctx) == 0) {
            sysdep_save_fp_ctrl_state(&local_lf()->sf);
            __cilkrts_sync(&local_lf()->sf);
        } else {
            sanitizer_finish_switch_fiber();
        }
    }

    __cilkrts_leave_loop_frame(local_lf());
    // after the last frame we have left the cilkified region
    CILK_ASSERT(__cilkrts_need_to_cilkify || local_lf() == &lf);
}

__attribute__((noinline))
static void cilk_for_loop_helper_32(__cilk_abi_f32_t body, void *data, uint32_t end, unsigned int grain, int inclusive,
                                         __cilkrts_stack_frame *parent) {
    __cilkrts_stack_frame sf;
    // the helper only calls cilk_for_loop, which sets up its own frame
    __cilkrts_enter_frame_helper(&sf, parent, /* spawner */ false);
    __cilkrts_detach(&sf, parent);
    cilk_for_loop_32(body, data, end, grain, inclusive);
    __cilk_helper_epilogue(&sf, parent, /* spawner */ false);
}


// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_spread_helper32(void *data, __cilk_abi_f32_t body, uint32_t g2, uint32_t r2,
                                    __cilkrts_loop_frame *parent) {
    FORCE_VAR_SIZED_FRAME();
    uint64_t i; // chunk index
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf, parent);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, parent, &i);
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf, &parent->sf); // push the parent loop_frame to the deque

        do {
            CILK_ASSERT(i + 1 == parent->start);
            uint32_t lo = (uint32_t)(i * g2 + (i < r2 ? i : r2));
            body(data, lo, lo + g2 + (i < r2));
            status = __cilkrts_loop_frame_next(&inner_lf, parent);
            i++;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        uint32_t lo = (uint32_t)(i * g2 + (i < r2 ? i : r2));
        body(data, lo, lo + g2 + (i < r2));
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    get_worker_from_stack(&inner_lf.sf)->local_loop_frame = parent;
    CILK_ASSERT(local_lf() == parent);

    // The inner loop frame is not a spawner, so if it was never detached there
    // is nothing to pop.
    if (inner_lf.sf.flags & CILK_FRAME_DETACHED)
        __cilk_helper_epilogue(&inner_lf.sf, &parent->sf, /* spawner */ false);
}

static void cilk_for_loop_spread_32(__cilk_abi_f32_t body, void *data, uint32_t end, uint32_t g2, uint32_t r2) {
    FORCE_VAR_SIZED_FRAME();
    __cilkrts_loop_frame lf;
    __cilkrts_enter_loop_frame(&lf, 0, end);

    // Contains the setjmp. We continue to the loop helper in both setjmp cases.
    __cilk_prepare_spawn(&lf.sf);

    __cilkrts_cilk_loop_spread_helper32(data, body, g2, r2, current_loop_frame());

    CILK_ASSERT(local_lf()->start == local_lf()->end);

    if (!__cilkrts_synced(&local_lf()->sf)) {
        if (__builtin_setjmp(local_lf()->sf.ctx) == 0) {
            sysdep_save_fp_ctrl_state(&local_lf()->sf);
            __cilkrts_sync(&local_lf()->sf);
        } else {
            sanitizer_finish_switch_fiber();
        }
    }

    __cilkrts_leave_loop_frame(local_lf());
    // after the last frame we have left the cilkified region
    CILK_ASSERT(__cilkrts_need_to_cilkify || local_lf() == &lf);
}

static void cilk_for_impl_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain, int inclusive) {
    uint32_t end, rem;

    if (grain == 1) {
        end = count;
        rem = 0;
    } else if (((grain - 1) & grain) == 0) { // power of 2 grainsize
        end = count >> __builtin_ctz(grain); // trailing zeroes intrinsic
        rem = count & (grain - 1);
    } else {
        end = count / grain, rem = count % grain;
    }

    // sanity check
    CILK_ASSERT(end == count / grain);
    CILK_ASSERT(rem == count % grain);

    // spread the remainder over the chunks (see "Spreading the remainder")
    if (rem != 0 && end != 0 && !inclusive) {
        uint32_t g2 = grain + rem / end, r2 = rem % end;
        return cilk_for_loop_spread_32(body, data, end, g2, r2);
    }

    // if inclusive, remainder always contains at least an iteration
    if (rem == 0 && !inclusive) {
        // no remainder to spawn
        return cilk_for_loop_32(body, data, end, grain, inclusive);
    }

    // spawn the body, continue with the remainder
    FORCE_VAR_SIZED_FRAME();
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame(&sf);

    // cilk_spawn cilk_for_loop_32(body, data, end, grain, inclusive);
    if(!__cilk_prepare_spawn(&sf)) {
        cilk_for_loop_helper_32(body, data, end, grain, inclusive, &sf);
    }

    // remainder is the continuation
    body(data, count - rem, count);

    // cilk_sync
    __cilk_sync_nothrow(&sf);
    __cilk_parent_epilogue(&sf);
}