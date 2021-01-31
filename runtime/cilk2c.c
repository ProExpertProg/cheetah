#include <stdio.h>

#include "debug.h"

#include "cilk2c.h"
#include "cilk-internal.h"
#include "fiber.h"
#include "membar.h"
#include "scheduler.h"
#include "readydeque.h"

int cilkg_nproc = 0;

// ================================================================
// This file comtains the compiler ABI, which corresponds to 
// conceptually what the compiler generates to implement Cilk code.
// They are included here in part as documentation, and in part
// allow one to write and run "hand-compiled" Cilk code.
// ================================================================

// inlined by the compiler
void __cilkrts_enter_frame(__cilkrts_stack_frame *sf) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_enter_frame) frame %p\n", w->self, sf);

    sf->flags = CILK_FRAME_VERSION;
    sf->call_parent = w->current_stack_frame; 
    sf->worker = w;
    w->current_stack_frame = sf;
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);
    CILK_ASSERT(w, !__cilkrts_is_loop(sf));
}

// inlined by the compiler; this implementation is only used in invoke-main.c
void __cilkrts_enter_frame_fast(__cilkrts_stack_frame * sf) {
    __cilkrts_worker * w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_enter_frame_fast) frame %p\n", w->self, sf);

    sf->flags = CILK_FRAME_VERSION;
    sf->call_parent = w->current_stack_frame; 
    sf->worker = w;
    w->current_stack_frame = sf;
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);
    CILK_ASSERT(w, !__cilkrts_is_loop(sf));
}

// inlined by the compiler
void __cilkrts_enter_loop_frame(__cilkrts_loop_frame * lf, __uint64_t start, __uint64_t end) {
    __cilkrts_worker * w = __cilkrts_get_tls_worker();
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_enter_loop_frame) frame %p\n", w->self, lf);

    lf->sf.flags = CILK_FRAME_VERSION | CILK_FRAME_LOOP;
    lf->sf.call_parent = w->current_stack_frame;
    lf->sf.worker = w;

    lf->start = start;
    lf->end = end;

    w->current_stack_frame = &lf->sf;
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);
    CILK_ASSERT(w, __cilkrts_is_loop(&lf->sf));

    // save so we can access it later when we're stackless
    w->local_loop_frame = lf;
}

// inlined by the compiler
void __cilkrts_enter_inner_loop_frame(__cilkrts_inner_loop_frame *lf) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();

    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_enter_inner_loop_frame) frame %p\n", w->self, lf);
    CILK_ASSERT(w, __cilkrts_is_loop(w->current_stack_frame));

    lf->sf.flags = CILK_FRAME_VERSION | CILK_FRAME_INNER_LOOP;
    lf->sf.call_parent = w->current_stack_frame;
    WHEN_CILK_DEBUG(lf->parentLF = (__cilkrts_loop_frame *) w->current_stack_frame);
    lf->sf.worker = w;

    w->current_stack_frame = &lf->sf;

    CILK_ASSERT(w, __cilkrts_is_loop(lf->sf.call_parent));
    CILK_ASSERT(w, !__cilkrts_is_loop(&lf->sf));
    CILK_ASSERT(w, __cilkrts_is_inner_loop(&lf->sf));
    CILK_ASSERT(w, (lf->sf.flags & CILK_FRAME_DETACHED) == 0);
    // WHEN_CILK_DEBUG(sf->magic = CILK_STACKFRAME_MAGIC);
}

// inlined by the compiler
// this function should ONLY be called for the first iteration, before we push the loop frame on the deque
__cilkrts_iteration_return __cilkrts_grab_first_iteration(__cilkrts_inner_loop_frame *lf, __uint64_t *index) {

    WHEN_CILK_DEBUG(__cilkrts_worker * w = __cilkrts_get_tls_worker());
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_grab_first_iteration) frame %p\n", w->self, lf);
    CILK_ASSERT(w, w == lf->sf.worker);
    CILK_ASSERT(w, __cilkrts_is_inner_loop(&lf->sf));
    CILK_ASSERT(w, __cilkrts_is_loop(lf->sf.call_parent));

    // We must currently be in the inner loop frame
    CILK_ASSERT(w, &lf->sf == w->current_stack_frame);

    // this frame shouldn't be detached yet
    CILK_ASSERT(w, (lf->sf.flags & CILK_FRAME_DETACHED) == 0);

    // get the first iteration from loopframe
    __cilkrts_loop_frame *pLoopFrame = (__cilkrts_loop_frame *) lf->sf.call_parent;
    CILK_ASSERT(w, pLoopFrame == lf->parentLF);

    *index = pLoopFrame->start++;
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_grab_iteration) Iteration %d unconfirmed\n", w->self, *index);

    if (pLoopFrame->start > pLoopFrame->end) {
        __cilkrts_alert(ALERT_LOOP, "[%d]: (__cilkrts_grab_iteration) Loop ending at i=%i\n", w->self, *index);
        pLoopFrame->start--;
        return FAIL;
    } else if (pLoopFrame->start == pLoopFrame->end) {
        __cilkrts_alert(ALERT_LOOP, "[%d]: (__cilkrts_grab_iteration) Only iteration with i=%i\n", w->self, *index);
        return SUCCESS_LAST_ITERATION;
    } else {
        return SUCCESS_ITERATION;
    }

}

// inlined by the compiler; this implementation is only used in invoke-main.c
void __cilkrts_detach(__cilkrts_stack_frame * sf) {
    struct __cilkrts_worker *w = sf->worker;
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_detach) frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    CILK_ASSERT(w, w->current_stack_frame == sf);

    struct __cilkrts_stack_frame *parent = sf->call_parent;
    struct __cilkrts_stack_frame *volatile *tail = w->tail;
    CILK_ASSERT(w, (tail+1) < w->ltq_limit);

    // store parent at *tail, and then increment tail
    *tail++ = parent;
    sf->flags |= CILK_FRAME_DETACHED;
    Cilk_membar_StoreStore();
    w->tail = tail;
}

// inlined by the compiler; this implementation is only used in invoke-main.c
void __cilkrts_save_fp_ctrl_state(__cilkrts_stack_frame *sf) {
    sysdep_save_fp_ctrl_state(sf);
}

void __cilkrts_sync(__cilkrts_stack_frame *sf) {

    __cilkrts_worker *w = sf->worker;
    __cilkrts_alert(ALERT_SYNC, "[%d]: (__cilkrts_sync) syncing frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, sf == w->current_stack_frame);

    if( Cilk_sync(w, sf) == SYNC_READY ) {
        // the sync could reallocate the LoopFrame.
        if (sf != w->current_stack_frame) {
            // sf is not a valid location anymore
            CILK_ASSERT(w, __cilkrts_is_loop(w->current_stack_frame));
            CILK_ASSERT(w, w->current_stack_frame == &w->local_loop_frame->sf);
            sf = w->current_stack_frame;
        }

        __cilkrts_alert(ALERT_SYNC, 
            "[%d]: (__cilkrts_sync) synced frame %p!\n", w->self, sf);
        // The Cilk_sync restores the original rsp stored in sf->ctx
        // if this frame is ready to sync.
        sysdep_longjmp_to_sf(sf);
    } else {
        __cilkrts_alert(ALERT_SYNC, 
            "[%d]: (__cilkrts_sync) waiting to sync frame %p!\n", w->self, sf);
        longjmp_to_runtime(w);                        
    }
}

__cilkrts_iteration_return __cilkrts_pop_loop_frame(__cilkrts_inner_loop_frame *lf) {
    __cilkrts_worker *w = lf->sf.worker;
    __cilkrts_alert(ALERT_CFRAME,
                    "[%d]: (__cilkrts_pop_loop_frame) attempting to obtain another iteration for frame %p\n",
                    w->self, lf);
    CILK_ASSERT(w, lf->sf.flags & CILK_FRAME_VERSION);
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


// inlined by the compiler; this implementation is only used in invoke-main.c
void __cilkrts_pop_frame(__cilkrts_stack_frame * sf) {
    __cilkrts_worker * w = sf->worker;
    __cilkrts_alert(ALERT_CFRAME, "[%d]: (__cilkrts_pop_frame) frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    w->current_stack_frame = sf->call_parent;
    sf->call_parent = 0;
}

void __cilkrts_leave_frame(__cilkrts_stack_frame * sf) {

    __cilkrts_worker *w = sf->worker;
    __cilkrts_alert(ALERT_CFRAME, 
        "[%d]: (__cilkrts_leave_frame) leaving frame %p\n", w->self, sf);

    CILK_ASSERT(w, sf->flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, sf->worker == __cilkrts_get_tls_worker());
    // WHEN_CILK_DEBUG(sf->magic = ~CILK_STACKFRAME_MAGIC);

    if(sf->flags & CILK_FRAME_DETACHED) { // if this frame is detached
        __cilkrts_stack_frame *volatile *t = w->tail;
        --t;
        w->tail = t;
        __sync_fetch_and_and(&sf->flags, ~CILK_FRAME_DETACHED);
        // Cilk_membar_StoreLoad(); // the sync_fetch_and_add replaced mfence
                // which is slightly more efficient.  Note that this
                // optimiation is applicable *ONLY* on i386 and x86_64
        if(__builtin_expect(w->exc > t, 0)) {
            // this may not return if last work item has been stolen
            Cilk_exception_handler(__cilkrts_is_inner_loop(sf));
        }
        // CILK_ASSERT(w, *(w->tail) == w->current_stack_frame);
    } else {
        // A detached frame would never need to call Cilk_set_return, which 
        // performs the return protocol of a full frame back to its parent
        // when the full frame is called (not spawned).  A spawned full 
        // frame returning is done via a different protocol, which is 
        // triggered in Cilk_exception_handler. 
        if(sf->flags & CILK_FRAME_STOLEN) { // if this frame has a full frame
            __cilkrts_alert(ALERT_RETURN, 
                "[%d]: (__cilkrts_leave_frame) parent is call_parent!\n", w->self);
            // leaving a full frame; need to get the full frame of its call
            // parent back onto the deque
            Cilk_set_return(w);
            CILK_ASSERT(w, w->current_stack_frame->flags & CILK_FRAME_VERSION);
        }
    }
}

// lf != w->current_stack_frame because we just executed pop
void __cilkrts_leave_loop_frame(__cilkrts_loop_frame * lf) {

    __cilkrts_worker *w = lf->sf.worker;
    __cilkrts_alert(ALERT_CFRAME,
        "[%d]: (__cilkrts_leave_loop_frame) leaving frame %p\n", w->self, lf);

    CILK_ASSERT(w, lf->sf.flags & CILK_FRAME_VERSION);
    CILK_ASSERT(w, __cilkrts_is_loop(&lf->sf));
    CILK_ASSERT(w, lf->sf.worker == __cilkrts_get_tls_worker());
    CILK_ASSERT(w, lf == w->local_loop_frame);

    // We've already passed the cilk_sync
    CILK_ASSERT(w, __cilkrts_synced(&lf->sf));

    // WHEN_CILK_DEBUG(sf->magic = ~CILK_STACKFRAME_MAGIC);

    if(lf->sf.flags & CILK_FRAME_SPLIT) {
        // if this frame is split

        // this is the equivalent of a DETACHED standard frame, except that
        // the THE protocol is unnecessary. Rather, we behave as if we've
        // already failed the THE protocol (the frame is split, so somebody
        // stole the remaining iterations). We simply end the current closure
        // and attempt a provably good steal on the parent.

        // this is pointing at our frame parent, but our closure parent is
        // another LoopFrame, so let's unset to avoid confusion.
        w->current_stack_frame = NULL;

        Cilk_loop_frame_return();
    } else {
        // This loop frame is not split, which means it is the last one remaining
        // (that is certain because we've successfully passed the cilk_sync).
        // Hence, this protocol is the same as the one for a non-DETACHED standard frame.
        if(lf->sf.flags & CILK_FRAME_STOLEN) { // if this frame has a full frame
            __cilkrts_alert(ALERT_RETURN,
                "[%d]: (__cilkrts_leave_frame) parent is call_parent!\n", w->self);
            // leaving a full frame; need to get the full frame of its call
            // parent back onto the deque
            Cilk_set_return(w);
            CILK_ASSERT(w, w->current_stack_frame->flags & CILK_FRAME_VERSION);
            CILK_ASSERT(w, !__cilkrts_is_dynamic(&lf->sf));
        }
    }
}

__cilkrts_loop_frame * local_lf() {
    extern __thread __cilkrts_worker *tls_worker; // faster than a function call
    return tls_worker->local_loop_frame;
    // return __cilkrts_get_tls_worker()->local_loop_frame;
}

int __cilkrts_get_nworkers(void) {
  return cilkg_nproc;
}
