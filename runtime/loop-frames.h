//
// Created by luka on 4/25/20.
//

#ifndef CHEETAH_LOOP_FRAMES_H
#define CHEETAH_LOOP_FRAMES_H

#include "cilk-internal.h"

typedef enum __cilkrts_split_lf_result {
    SUCCESS = 0u,
    SUCCESS_REMOVE = 1u,
    NOT_LOOP_FRAME = 2u,
    FAIL = 3u
} __cilkrts_split_lf_result;

__cilkrts_split_lf_result
split_loop_frame(__cilkrts_stack_frame *frame_to_steal, __cilkrts_worker *w, __cilkrts_loop_frame **res_lf);

__cilkrts_loop_frame *clone_loop_frame(__cilkrts_loop_frame *loop_frame, __cilkrts_worker *w);

void sync_loop_frame(__cilkrts_worker *w, Closure *t);

#endif //CHEETAH_LOOP_FRAMES_H
