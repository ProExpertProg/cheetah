//
// Created by luka on 4/25/20.
//

#include "loop-frames.h"
#include <string.h>

// returns the new loop frame on success, NULL on failure
__cilkrts_loop_frame *
split_loop_frame(__cilkrts_stack_frame *frame_to_steal, __cilkrts_worker *w, struct cilk_fiber *fiber) {
    if (__cilkrts_is_loop(frame_to_steal)) {
        __cilkrts_loop_frame *lf = (__cilkrts_loop_frame *) frame_to_steal;
        uint64_t len = lf->end - lf->start;
        if (len > 1) {
            // split the frame in half

            uint64_t mid = lf->start + len / 2;
            __cilkrts_loop_frame *new_lf = clone_loop_frame(lf, w, fiber);

            __cilkrts_alert(ALERT_LOOP, "[%d]: (split_loop_frame) Splitting frame %p [%d:%d] in two (new_lf=%p)!\n", w->self, lf, lf->start, lf->end, new_lf);

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
    char *new_lf = fiber->m_stack_base - sizeof(__cilkrts_loop_frame);
    memcpy(new_lf, loop_frame, sizeof(__cilkrts_loop_frame));
    fiber->has_loop_frame = 1;
    return (__cilkrts_loop_frame *) new_lf;
}
