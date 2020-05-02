//
// Created by luka on 4/25/20.
//

#include "closure.h"
#include "loop-frames.h"
#include <string.h>

// returns the new loop frame on success, NULL on failure
__cilkrts_loop_frame *split_loop_frame(__cilkrts_stack_frame *frame_to_steal, __cilkrts_worker *w) {
    if (__cilkrts_is_loop(frame_to_steal)) {
        __cilkrts_loop_frame *lf = (__cilkrts_loop_frame *) frame_to_steal;
        uint64_t len = lf->end - lf->start;
        if (len > 1) {
            // split the frame in half

            uint64_t mid = lf->start + len / 2;
            __cilkrts_loop_frame *new_lf = clone_loop_frame(lf, w);

            __cilkrts_alert(ALERT_LOOP, "[%d]: (split_loop_frame) Splitting frame %p [%d:%d] in two (new_lf=%p)!\n",
                            w->self, lf, lf->start, lf->end, new_lf);

            CILK_ASSERT(w, new_lf->end == lf->end);
            lf->end = mid;
            new_lf->start = mid;


            // The old frame is now split (it could be split before,
            // in which case the new one is also split).
            __cilkrts_set_split(lf);
            __cilkrts_set_synced(&lf->sf);
            // Because the thief takes the old closure and new frame,
            // the old frame goes into a new child closure,
            // with no outstanding children, so it's synced

            return new_lf;
        }

        __cilkrts_alert(ALERT_LOOP, "(split_loop_frame) Stealing the whole loop frame %p [%d:%d]!\n", lf, lf->start, lf->end);
    }
    return NULL;
}

__cilkrts_loop_frame *
clone_loop_frame(__cilkrts_loop_frame *loop_frame, __cilkrts_worker *w) {
    __cilkrts_loop_frame *new_lf = cilk_internal_malloc(w, sizeof(__cilkrts_loop_frame));
    memcpy(new_lf, loop_frame, sizeof(__cilkrts_loop_frame));
    __cilkrts_set_dynamic(new_lf);
    return new_lf;
}

// Copy important fields of current into original, as we prepare to free current and use original
// Assert equality of important fields that we don't explicitly set and care about.
static void copy_loop_frame(__cilkrts_worker *w, __cilkrts_loop_frame *original, __cilkrts_loop_frame *current) {

    CILK_ASSERT(w, __cilkrts_is_dynamic(&current->sf));
    CILK_ASSERT(w, !__cilkrts_is_dynamic(&original->sf));

    CILK_ASSERT(w, __cilkrts_is_split(&original->sf)); // current could be either
    if(!__cilkrts_is_split(&current->sf))
        __cilkrts_set_nonsplit(original);

    CILK_ASSERT(w, __cilkrts_is_split(&original->sf) == __cilkrts_is_split(&current->sf));

    CILK_ASSERT(w, __cilkrts_synced(&original->sf));
    CILK_ASSERT(w, __cilkrts_unsynced(&current->sf)); // will get set to synced very soon though

    CILK_ASSERT(w, __cilkrts_stolen(&current->sf)); // can't say anything about original
    __cilkrts_set_stolen(&original->sf);

    uint32_t flag_mask = ~(CILK_FRAME_DYNAMIC | CILK_FRAME_SPLIT | CILK_FRAME_UNSYNCHED | CILK_FRAME_STOLEN);

    // Make sure this doesn't break if size of flags changes
    CILK_ASSERT(w, sizeof(original->sf.flags) == sizeof(flag_mask));

    uint32_t original_flags = original->sf.flags & flag_mask;
    uint32_t current_flags = current->sf.flags & flag_mask;
    CILK_ASSERT(w, original_flags == current_flags);

    CILK_ASSERT(w, original->sf.call_parent == NULL); // was set to null in pop_frame
    CILK_ASSERT(w, current->sf.call_parent != NULL); // we're at the sync, cannot be NULL yet

    original->sf.call_parent = current->sf.call_parent;

    // worker gets set right after this, in setup_for_sync

    // sp gets set to ORIG_RSP so we don't care.
    memcpy(original->sf.ctx, current->sf.ctx, sizeof(jmpbuf));

    original->sf.mxcsr = current->sf.mxcsr;
    original->sf.fpcsr = current->sf.fpcsr;

    WHEN_CILK_DEBUG(original->sf.magic = current->sf.magic);

    CILK_ASSERT(w, original->start == original->end);
    CILK_ASSERT(w, current->start == current->end);
    CILK_ASSERT(w, current->end != original->end);
    // we don't care about start and end, they aren't used anymore.
}

// Used in setup_for_sync to make sure we are using the correct memory for our loop frame.
// In scenario a), the is in the middle of successfully completing a sync
// In scenario b), the last child is successfully performing a provably good steal.
void sync_loop_frame(__cilkrts_worker *w, Closure *t) {
    if (!t->most_original_loop_frame) {
        // we either already have the most original LoopFrame (there were oly inner loop
        // frame children), or we're not the last sync for this loop, so we can keep the
        // dynamically allocated one.

        CILK_ASSERT(w, !__cilkrts_is_dynamic(t->frame)
                       || __cilkrts_is_split(t->frame));
    } else {
        // This is the original LoopFrame, which we want to use from now on.
        // Even if this is not the final sync, we wanna pass this frame to the parent.
        CILK_ASSERT(w, &t->most_original_loop_frame->sf != t->frame);

        copy_loop_frame(w, t->most_original_loop_frame, (__cilkrts_loop_frame *) t->frame);

        if (t->fiber) {
            __cilkrts_alert(ALERT_SYNC, "[%d]: (sync_loop_frame) Scenario 2.a), closure %p\n", w->self, t);
            // in case a), we still haven't freed our fiber
            CILK_ASSERT(w, w->current_stack_frame == t->frame);
        } else {
            __cilkrts_alert(ALERT_SYNC, "[%d]: (sync_loop_frame) Scenario 2.b), closure %p\n", w->self, t);
            // in case b), we've already freed our fiber, so we're returning from the child
            // hence, current_stack_frame is NULL

            CILK_ASSERT(w, w->current_stack_frame == NULL);
        }

        cilk_internal_free(w, t->frame, sizeof(__cilkrts_loop_frame));

        t->frame = &t->most_original_loop_frame->sf;

    }
    // need to set this in case b) or if we just switched to the original
    // - otherwise it's already set and there's no harm.
    w->current_stack_frame = t->frame;
    w->local_loop_frame = (__cilkrts_loop_frame *) t->frame;
}