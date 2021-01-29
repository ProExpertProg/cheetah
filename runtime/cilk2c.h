#ifndef _CILK2C_H
#define _CILK2C_H

#include <stdlib.h>
#include "cilk-internal.h"

// mainly used by invoke-main.c
extern unsigned long ZERO;

// These functoins are mostly inlined by the compiler, except for 
// __cilkrts_leave_frame.  However, their implementations are also
// provided in cilk2c.c.  The implementations in cilk2c.c are used 
// by invoke-main.c and can be used to "hand compile" cilk code.
void __cilkrts_enter_frame(__cilkrts_stack_frame *sf);
void __cilkrts_enter_frame_fast(__cilkrts_stack_frame * sf);

// The return type of the functions obtaining iterations
typedef enum __cilkrts_iteration_return {
    SUCCESS_ITERATION = 0u,
    SUCCESS_LAST_ITERATION = 1u,
    FAIL = 2u
} __cilkrts_iteration_return;

void __cilkrts_enter_loop_frame(__cilkrts_loop_frame * lf, __uint64_t start, __uint64_t end);
void __cilkrts_enter_inner_loop_frame(__cilkrts_inner_loop_frame *lf);
__cilkrts_iteration_return __cilkrts_grab_first_iteration(__cilkrts_inner_loop_frame * lf, __uint64_t *index);

void __cilkrts_save_fp_ctrl_state(__cilkrts_stack_frame *sf);
void __cilkrts_detach(__cilkrts_stack_frame * self);
void __cilkrts_sync(__cilkrts_stack_frame *sf);

__cilkrts_iteration_return __cilkrts_pop_loop_frame(__cilkrts_inner_loop_frame *lf, __uint64_t * index);

void __cilkrts_pop_frame(__cilkrts_stack_frame * sf);
void __cilkrts_leave_frame(__cilkrts_stack_frame * sf);
void __cilkrts_leave_loop_frame(__cilkrts_loop_frame * sf);
int __cilkrts_get_nworkers(void);
#endif
