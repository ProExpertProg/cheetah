//
// Created by luka on 4/25/20.
//

#ifndef CHEETAH_LOOP_FRAMES_H
#define CHEETAH_LOOP_FRAMES_H

#include "cilk-internal.h"
__cilkrts_loop_frame * split_loop_frame(__cilkrts_stack_frame *frame_to_steal, __cilkrts_worker *w, struct cilk_fiber *fiber);

__cilkrts_loop_frame *clone_loop_frame(__cilkrts_loop_frame *loop_frame, __cilkrts_worker *w, struct cilk_fiber *fiber);

char *get_loop_frame_address(const struct cilk_fiber *fiber);

__cilkrts_loop_frame *allocate_temp_loop_frame(__cilkrts_worker *w);

void sync_loop_frame(__cilkrts_worker *w, Closure *t);

#endif //CHEETAH_LOOP_FRAMES_H
