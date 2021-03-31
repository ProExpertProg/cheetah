#ifndef _CILK2C_H
#define _CILK2C_H

#include "cilk-internal.h"
#include <stdlib.h>

// ABI functions inlined by the compiler (provided as a bitcode file after
// compiling runtime) are defined in cilk2c_inline.c.
// ABI functions not inlined by the compiler are defined in cilk2c.c.
CHEETAH_API void __cilkrts_enter_frame(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_enter_frame_fast(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_save_fp_ctrl_state(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_detach(__cilkrts_stack_frame *self);
CHEETAH_API void __cilkrts_check_exception_raise(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_check_exception_resume(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_cleanup_fiber(__cilkrts_stack_frame *, int32_t sel);
CHEETAH_API void __cilkrts_sync(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_pop_frame(__cilkrts_stack_frame *sf);
CHEETAH_API void __cilkrts_pause_frame(__cilkrts_stack_frame *sf, char *exn);
CHEETAH_API void __cilkrts_leave_frame(__cilkrts_stack_frame *sf);

// The return type of the functions obtaining iterations
typedef enum __cilkrts_iteration_return {
    SUCCESS_ITERATION = 0u,
    SUCCESS_LAST_ITERATION = 1u,
    FAIL = 2u
} __cilkrts_iteration_return;

CHEETAH_API void __cilkrts_enter_loop_frame(__cilkrts_loop_frame * lf, __uint64_t start, __uint64_t end);
CHEETAH_API void __cilkrts_enter_inner_loop_frame(__cilkrts_inner_loop_frame *lf);
CHEETAH_API __cilkrts_iteration_return __cilkrts_grab_first_iteration(__cilkrts_inner_loop_frame * lf, __uint64_t *index);
CHEETAH_API __cilkrts_iteration_return __cilkrts_loop_frame_next(__cilkrts_inner_loop_frame *lf);
CHEETAH_API void __cilkrts_leave_loop_frame(__cilkrts_loop_frame * sf);

CHEETAH_API __cilkrts_loop_frame * local_lf();

// Not marked as CHEETAH_API as it may be deprecated soon
unsigned __cilkrts_get_nworkers(void);
//CHEETAH_API int64_t* __cilkrts_get_pedigree(void);
//void __cilkrts_pedigree_bump_rank(void);
#endif
