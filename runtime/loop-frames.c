//
// Created by luka on 4/25/20.
//

#include "closure.h"
#include "loop-frames.h"
#include <string.h>

// returns the new loop frame on success, NULL on failure
__cilkrts_loop_frame *split_loop_frame(__cilkrts_stack_frame *frame_to_steal,
                                       __cilkrts_worker *w, struct cilk_fiber *fiber) {
    if (__cilkrts_is_loop(frame_to_steal)) {
        __cilkrts_loop_frame *lf = (__cilkrts_loop_frame *) frame_to_steal;
        uint64_t len = lf->end - lf->start;
        if (len > 1) {
            // split the frame in half

            uint64_t mid = lf->start + len / 2;
            __cilkrts_loop_frame *new_lf = clone_loop_frame(lf, w, fiber);

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
clone_loop_frame(__cilkrts_loop_frame *loop_frame, __cilkrts_worker *w, struct cilk_fiber *fiber) {
    char *new_lf = get_loop_frame_address(fiber);
    memcpy(new_lf, loop_frame, sizeof(__cilkrts_loop_frame));
    fiber->has_loop_frame = 1;
    return (__cilkrts_loop_frame *) new_lf;
}

char *get_loop_frame_address(const struct cilk_fiber *fiber) {
    return fiber->m_stack_base - sizeof(__cilkrts_loop_frame);
}

__cilkrts_loop_frame *allocate_temp_loop_frame(__cilkrts_worker *w) {
    return cilk_internal_malloc(w, sizeof(__cilkrts_loop_frame));
}


// Used in setup_for_sync to make sure we are using the correct memory for our loop frame.
// In scenario a), the is in the middle of successfully completing a sync
// In scenario b), the last child is successfully performing a provably good steal.
void sync_loop_frame(__cilkrts_worker *w, Closure *t) {
    if (!t->most_original_loop_frame) {
        // Scenario 1: we already have the correct LoopFrame;
        // there was only an inner loop frame child
        if (t->fiber) {
            // a)
            __cilkrts_alert(ALERT_SYNC, "[%d]: (sync_loop_frame) Scenario 1.a), closure %p\n", w->self, t);

            CILK_ASSERT(w, t->fiber->has_loop_frame == 0);
            CILK_ASSERT(w, (char *) t->frame != get_loop_frame_address(t->fiber));
            // t->frame is either local or on child_fiber
        } else {
            __cilkrts_alert(ALERT_SYNC, "[%d]: (sync_loop_frame) Scenario 1.b), closure %p\n", w->self, t);
        }
    } else {
        // Scenario 2: the LoopFrame we want to use from now on is most_original_loop_frame

        // most_original_loop_frame could be either
        if (t->fiber_child->has_loop_frame == 1) {
            CILK_ASSERT(w, (char *) t->most_original_loop_frame
                           == get_loop_frame_address(t->fiber_child));
        }

        // TODO Perhaps no need to copy everything over.
        memcpy(t->most_original_loop_frame, t->frame, sizeof(__cilkrts_loop_frame));

        if (t->fiber) {
            __cilkrts_alert(ALERT_SYNC, "[%d]: (sync_loop_frame) Scenario 2.a), closure %p\n", w->self, t);
            // in case a), we still have our fiber but not our temp loop frame

            CILK_ASSERT(w, t->temp_loop_frame == NULL);
            // that means our current frame is on the current fiber.
            CILK_ASSERT(w, t->fiber->has_loop_frame == 1);
            CILK_ASSERT(w, (char *) t->frame == get_loop_frame_address(t->fiber));

            CILK_ASSERT(w, w->current_stack_frame == t->frame);

            w->current_stack_frame = NULL;
            // we just copied the frame, so fiber is free to be freed
            t->fiber->has_loop_frame = 0;

        } else {
            __cilkrts_alert(ALERT_SYNC, "[%d]: (sync_loop_frame) Scenario 2.b), closure %p\n", w->self, t);
            // in case b), we don't have our own fiber but we have a temp_loop_frame

            CILK_ASSERT(w, t->temp_loop_frame != NULL);
            CILK_ASSERT(w, &t->temp_loop_frame->sf == t->frame);

            CILK_ASSERT(w, w->current_stack_frame == NULL);

            cilk_internal_free(w, t->temp_loop_frame, sizeof(__cilkrts_loop_frame));
        }

        t->frame = &t->most_original_loop_frame->sf;
        w->current_stack_frame = t->frame;

        w->local_loop_frame = t->most_original_loop_frame;
    }
}