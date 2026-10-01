//
// Created by luka on 4/25/20.
//

#include "closure.h"
#include "loop-frames.h"
#include <string.h>

// returns the new loop frame on success, NULL on failure
__cilkrts_split_lf_result
split_loop_frame(__cilkrts_stack_frame *frame_to_steal, __cilkrts_worker *w, __cilkrts_loop_frame **res_lf) {
    if (__cilkrts_is_loop(frame_to_steal)) {
        __cilkrts_loop_frame *lf = (__cilkrts_loop_frame *) frame_to_steal;

        // split the frame in half
        __cilkrts_loop_frame *new_lf = clone_loop_frame(lf, w);
        __uint64_t start = __atomic_load_n(&lf->start, __ATOMIC_RELAXED);
        cilkrts_alert(LOOP | ALERT_STEAL, "(split_loop_frame) Splitting frame %p [%lu:%lu] on victim %d in two (new_lf=%p)!", (void *)lf, start, lf->end, lf->sf.fh->worker->self, (void *)new_lf);
        CILK_ASSERT(new_lf->end == lf->end);

        uint64_t mid = (start + lf->end) / 2;
        new_lf->start = mid;
        __atomic_store_n(&lf->end, mid, __ATOMIC_SEQ_CST);

        start = __atomic_load_n(&lf->start, __ATOMIC_SEQ_CST);
        if (start > mid) {
            lf->end = new_lf->end;
            cilk_internal_free(w, new_lf, sizeof(__cilkrts_loop_frame), IM_LOOP_FRAMES);
            return FAIL;
        }

        // success!
        // check whether to retract E or remove the Loop Frame
        int retractExc = lf->start < lf->end;

        cilkrts_alert(LOOP | ALERT_STEAL, "(split_loop_frame) Restoring head: %d, mid=%lu", retractExc, mid);
        // only now we can manipulate flags on the worker's frame

        // The old frame is now split (it could be split before,
        // in which case the new one is also split).
        __cilkrts_set_split(lf);

        // Because the thief takes the old closure and new frame,
        // the old frame goes into a new child closure,
        // with no outstanding children, so it's synced
        __cilkrts_set_synced(&lf->sf);
        *res_lf = new_lf;
        return retractExc ? SUCCESS : SUCCESS_REMOVE;
    }
    return NOT_LOOP_FRAME;
}

__cilkrts_loop_frame *
clone_loop_frame(__cilkrts_loop_frame *loop_frame, __cilkrts_worker *w) {
    __cilkrts_loop_frame *new_lf = cilk_internal_malloc(w, sizeof(__cilkrts_loop_frame), IM_LOOP_FRAMES);
    memcpy(new_lf, loop_frame, sizeof(__cilkrts_loop_frame));
    __cilkrts_set_dynamic(new_lf);
    return new_lf;
}

// Copy important fields of current into original, as we prepare to free current and use original
// Assert equality of important fields that we don't explicitly set and care about.
static void copy_loop_frame(__cilkrts_loop_frame *original, __cilkrts_loop_frame *current) {

    CILK_ASSERT(__cilkrts_is_dynamic(&current->sf));
    CILK_ASSERT(!__cilkrts_is_dynamic(&original->sf));

    CILK_ASSERT(__cilkrts_is_split(&original->sf)); // current could be either
    if(!__cilkrts_is_split(&current->sf))
        __cilkrts_set_nonsplit(original);

    CILK_ASSERT(__cilkrts_is_split(&original->sf) == __cilkrts_is_split(&current->sf));

    CILK_ASSERT(__cilkrts_synced(&original->sf));
    CILK_ASSERT(!__cilkrts_synced(&current->sf)); // will get set to synced very soon though

    CILK_ASSERT(__cilkrts_stolen(&current->sf)); // can't say anything about original
    __cilkrts_set_stolen(&original->sf);

    uint32_t flag_mask = ~(CILK_FRAME_DYNAMIC | CILK_FRAME_SPLIT | CILK_FRAME_UNSYNCHED | CILK_FRAME_STOLEN);

    // Make sure this doesn't break if size of flags changes
    CILK_ASSERT(sizeof(original->sf.flags) == sizeof(flag_mask));

    uint32_t original_flags = original->sf.flags & flag_mask;
    uint32_t current_flags = current->sf.flags & flag_mask;
    USE_UNUSED(original_flags);
    USE_UNUSED(current_flags);
    CILK_ASSERT(original_flags == current_flags);

    CILK_ASSERT(original->sf.call_parent == NULL); // was set to null in pop_frame
    CILK_ASSERT(current->sf.call_parent != NULL); // we're at the sync, cannot be NULL yet

    original->sf.call_parent = current->sf.call_parent;

    // fh (and thus worker) gets set right after this, in setup_for_sync

    // sp gets set to ORIG_RSP so we don't care.
    memcpy(original->sf.ctx, current->sf.ctx, sizeof(jmpbuf));

    original->sf.magic = current->sf.magic;


    CILK_ASSERT(original->start == original->end);
    CILK_ASSERT(current->start == current->end);
    // if current was stolen from original when original was empty, their ends could match
    // we don't care about start and end, they aren't used anymore.

#ifdef ENABLE_CILKRTS_PEDIGREE
    TODO copy over data
#endif
}

// Used in setup_for_sync to make sure we are using the correct memory for our loop frame.
// In scenario a), the closure is in the middle of successfully completing a sync
// In scenario b), the last child is successfully performing a provably good steal.
//
// Must be called after setup_for_sync has switched t->fiber to the fiber that
// we are going to continue on (t->fiber_child). The frame we end up with must
// be fh->current_stack_frame of that fiber, so we set that here.
// had_fiber is true in scenario a) and false in scenario b).
void sync_loop_frame(__cilkrts_worker *w, Closure *t, bool had_fiber) {
    struct cilk_fiber *fh = t->fiber;
    CILK_ASSERT(fh);
    // only for pointer comparison, the frame might be freed below
    __cilkrts_stack_frame *old_frame = t->frame;
    USE_UNUSED(old_frame);

    if (!t->most_original_loop_frame) {
        // we either already have the most original LoopFrame (there were only inner loop
        // frame children), or we're not the last sync for this loop, so we can keep the
        // dynamically allocated one.

        CILK_ASSERT(!__cilkrts_is_dynamic(t->frame)
                    || __cilkrts_is_split(t->frame));
    } else {
        // This is the original LoopFrame, which we want to use from now on.
        // Even if this is not the final sync, we wanna pass this frame to the parent.
        CILK_ASSERT(&t->most_original_loop_frame->sf != t->frame);

        copy_loop_frame(t->most_original_loop_frame, (__cilkrts_loop_frame *) t->frame);

        cilkrts_alert(SYNC | ALERT_LOOP, "(sync_loop_frame) Scenario %s, closure %p",
                      had_fiber ? "2.a)" : "2.b)", (void *)t);

        cilk_internal_free(w, t->frame, sizeof(__cilkrts_loop_frame), IM_LOOP_FRAMES);

        t->frame = &t->most_original_loop_frame->sf;
        t->most_original_loop_frame = NULL; // we've used this, if the closure gets reused this needs to be cleared.
    }

    // The leftmost child that passed us this fiber left a NULL (or our old
    // frame) here, as there was no meaningful frame to set it to. Now there is.
    CILK_ASSERT(fh->current_stack_frame == NULL
                || fh->current_stack_frame == old_frame);
    fh->current_stack_frame = t->frame;
    w->local_loop_frame = (__cilkrts_loop_frame *) t->frame;
}
