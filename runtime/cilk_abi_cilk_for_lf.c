//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include <stdint.h>
#include "cilk2c.h"
#include "scheduler.h"
#include "readydeque.h"
#include "local.h"

#if INLINE_POP_LF
#define ATTR_POP_LF __always_inline
#else
#define ATTR_POP_LF __attribute_noinline__
#endif

ATTR_POP_LF __cilkrts_iteration_return __cilkrts_loop_frame_next(__cilkrts_inner_loop_frame *lf) {
    __cilkrts_worker *w = lf->sf.worker;
    cilkrts_alert(CFRAME,
                    w, "(__cilkrts_loop_frame_next) attempting to obtain another iteration for frame %p\n",
                    lf);
    CILK_ASSERT(w, CHECK_CILK_FRAME_MAGIC(w->g, &lf->sf));
    CILK_ASSERT(w, lf->sf.worker == __cilkrts_get_tls_worker());
    CILK_ASSERT(w, &lf->sf == w->current_stack_frame);

    CILK_ASSERT(w, lf->sf.flags & CILK_FRAME_DETACHED);
    CILK_ASSERT(w, __cilkrts_is_inner_loop(&lf->sf));
    CILK_ASSERT(w, __cilkrts_is_loop(lf->sf.call_parent));

    // THESE protocol

    __cilkrts_loop_frame *pLoopFrame = (__cilkrts_loop_frame *) lf->sf.call_parent;
    CILK_ASSERT(w, lf->parentLF == pLoopFrame);

    // safe to read tail as we're the only ones updating it
    // either:
    // - we're at the boundary of the stack (we stole something nested and are now walking back up,
    //   with access to the loop frame as but not on our deque (not ever)
    // - the loop frame is on our deque, whether we have access to it or not
    CILK_ASSERT(w, w->tail == w->l->shadow_stack || *(w->tail-1) == lf->sf.call_parent);

    // we just need to make sure that the load of end happens after store of start
    uint64_t start = __atomic_load_n(&pLoopFrame->start, __ATOMIC_RELAXED);
    start++;
    __atomic_store_n(&pLoopFrame->start, start, __ATOMIC_SEQ_CST);

    if (__builtin_expect(start > __atomic_load_n(&pLoopFrame->end, __ATOMIC_SEQ_CST), 0)) {
        deque_lock_self(w);
        // no need for a fence because we now have exclusive access
        // also, the lock already fenced (it should have??)
        if (start > __atomic_load_n(&pLoopFrame->end, __ATOMIC_SEQ_CST)) {
            pLoopFrame->start--;
            deque_unlock_self(w);
            return FAIL;
        }
        deque_unlock_self(w);
    }

    // TODO perhaps the optimization if LF is left empty

    return SUCCESS_ITERATION;
}

typedef void (*__cilk_abi_f64_t)(void *data, int64_t low, int64_t high);

// we cannot inline this function because of local variables
static void __attribute__ ((noinline))
__cilkrts_cilk_loop_helper64(void *data, __cilk_abi_f64_t body, unsigned int grainsize) {
    uint64_t i;
    __cilkrts_inner_loop_frame inner_lf;
    __cilkrts_enter_inner_loop_frame(&inner_lf);

    __cilkrts_iteration_return status = __cilkrts_grab_first_iteration(&inner_lf, &i);
    i *= grainsize;
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        do {
            CILK_ASSERT(__cilkrts_get_tls_worker(), i + grainsize == inner_lf.parentLF->start * grainsize);
            body(data, i, i + grainsize);
            status = __cilkrts_loop_frame_next(&inner_lf);
            i += grainsize;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        body(data, i, i + grainsize);
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    inner_lf.sf.worker->local_loop_frame = (__cilkrts_loop_frame *) inner_lf.sf.call_parent;
    CILK_ASSERT(__cilkrts_get_tls_worker(), local_lf() == inner_lf.parentLF);

    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
}

void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain) {
    __cilkrts_loop_frame lf;
    uint64_t end, rem;

    if (grain == 1) {
        end = count;
        rem = 0;
    } else if (((grain-1) & grain) == 0) { // power of 2 grainsize
        end = count >> __builtin_ctz(grain); // trailing zeroes intrinsic
        rem = count & (grain - 1);
    } else {
        end = count / grain, rem = count % grain;
    }

    // sanity check
    CILK_ASSERT(__cilkrts_get_tls_worker(), end == count / grain);
    CILK_ASSERT(__cilkrts_get_tls_worker(), rem == count % grain);

    __cilkrts_enter_loop_frame(&lf, 0, end);

    // cilk_for(int i = low; i < high; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    __builtin_setjmp(lf.sf.ctx); // the same behavior first time or not

    __cilkrts_cilk_loop_helper64(data, body, grain);

    CILK_ASSERT(lf.sf.worker, local_lf()->start == local_lf()->end);

//    __cilkrts_stack_frame ** p = alloca(sizeof(__cilkrts_stack_frame*));
//            *p = &local_lf()->sf;
//    // try alwaysinline/alloca?
    if (__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if (!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }

    __cilkrts_pop_frame(&local_lf()->sf);
    __cilkrts_leave_loop_frame(local_lf());

    CILK_ASSERT(lf.sf.worker, local_lf() == &lf);
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
    i *= grainsize;
    if (status == SUCCESS_ITERATION) {
        __cilkrts_detach(&inner_lf.sf); // push the parent loop_frame to the deque

        do {
            CILK_ASSERT(__cilkrts_get_tls_worker(), i + grainsize == inner_lf.parentLF->start * grainsize);
            body(data, i, i + grainsize);
            status = __cilkrts_loop_frame_next(&inner_lf);
            i += grainsize;
        } while (status == SUCCESS_ITERATION);
    }

    if (status == SUCCESS_LAST_ITERATION) {
        body(data, i, i + grainsize);
    }

    // local loop frame might have been modified if we have a nested loop inside loop body
    inner_lf.sf.worker->local_loop_frame = (__cilkrts_loop_frame *) inner_lf.sf.call_parent;

    __cilkrts_pop_frame(&inner_lf.sf);
    __cilkrts_leave_frame(&inner_lf.sf);
}

void __cilkrts_cilk_for_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain) {
    __cilkrts_loop_frame lf;
    uint32_t end, rem;

    if (grain == 1) {
        end = count;
        rem = 0;
    } else if (((grain-1) & grain) == 0) { // power of 2 grainsize
        end = count >> __builtin_ctz(grain); // trailing zeroes intrinsic
        rem = count & (grain - 1);
    } else {
        end = count / grain, rem = count % grain;
    }

    // sanity check
    CILK_ASSERT(__cilkrts_get_tls_worker(), end == count / grain);
    CILK_ASSERT(__cilkrts_get_tls_worker(), rem == count % grain);

    __cilkrts_enter_loop_frame(&lf, 0, end);

    // cilk_for(int i = low; i < high; ++i) {
    __cilkrts_save_fp_ctrl_state(&lf.sf);
    __builtin_setjmp(lf.sf.ctx); // the same behavior first time or not

    __cilkrts_cilk_loop_helper32(data, body, grain);

    if (__cilkrts_unsynced(&local_lf()->sf)) {
        __cilkrts_save_fp_ctrl_state(&local_lf()->sf);
        if (!__builtin_setjmp(local_lf()->sf.ctx)) {
            __cilkrts_sync(&local_lf()->sf);
        }
    }

    __cilkrts_pop_frame(&local_lf()->sf);
    __cilkrts_leave_loop_frame(local_lf());

    CILK_ASSERT(lf.sf.worker, local_lf() == &lf);
    if (rem != 0)
        body(data, count - rem, count);
}