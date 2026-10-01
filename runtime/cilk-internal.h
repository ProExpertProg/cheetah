#ifndef _CILK_INTERNAL_H
#define _CILK_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include <cilk/cilk_api.h>

#include "debug.h"
#include "fiber-header.h"
#include "frame.h"
#include "internal-malloc.h"
#include "rts-config.h"
#include "sched_stats.h"
#include "types.h"
#include "worker.h"

#if defined __i386__ || defined __x86_64__
#ifdef __SSE__
#define CHEETAH_SAVE_MXCSR
#endif
#endif

struct global_state;
typedef struct global_state global_state;
typedef struct local_state local_state;

struct cilkrts_callbacks {
    unsigned last_init;
    unsigned last_exit;
    bool after_init;
    void (*init[MAX_CALLBACKS])(void);
    void (*exit[MAX_CALLBACKS])(void);
};

extern CHEETAH_INTERNAL struct cilkrts_callbacks cilkrts_callbacks;

extern bool __cilkrts_use_extension;
#if ENABLE_EXTENSION
#define USE_EXTENSION __cilkrts_use_extension
#else
#define USE_EXTENSION false
#endif
extern __thread __cilkrts_worker *__cilkrts_tls_worker;
extern __thread struct cilk_fiber *__cilkrts_current_fh;
extern bool __cilkrts_need_to_cilkify;

static inline __attribute__((always_inline)) __cilkrts_worker *
__cilkrts_get_tls_worker(void) {
    return __cilkrts_tls_worker;
}

static inline __attribute__((always_inline)) __cilkrts_worker *
get_worker_from_stack(const __cilkrts_stack_frame *sf) {
    // Although we can get the current worker by calling
    // __cilkrts_get_tls_worker(), that method accesses the worker via TLS,
    // which can be slow on some systems.  This method gets the current worker
    // from the given __cilkrts_stack_frame, which is more efficient than a TLS
    // access on those systems.
    return sf->fh->worker;
}

CHEETAH_INTERNAL
void *internal_reducer_lookup(__cilkrts_worker *w, void *key, size_t size,
                              void *identity_ptr, void *reduce_ptr);
CHEETAH_INTERNAL
void internal_reducer_remove(__cilkrts_worker *w, void *key);

void __cilkrts_register_extension(void *extension);
void *__cilkrts_get_extension(void);
void __cilkrts_extend_spawn(__cilkrts_worker *w, void **parent_extension,
                            void **child_extension);
void __cilkrts_extend_return_from_spawn(__cilkrts_worker *w, void **extension);
void __cilkrts_extend_sync(void **extension);

static inline __attribute__((always_inline)) void *
__cilkrts_push_ext_stack(__cilkrts_worker *w, size_t size) {
    uint8_t *ext_stack_ptr = ((uint8_t *)w->ext_stack) - size;
    w->ext_stack = (void *)ext_stack_ptr;
    return ext_stack_ptr;
}

static inline __attribute__((always_inline)) void *
__cilkrts_pop_ext_stack(__cilkrts_worker *w, size_t size) {
    uint8_t *ext_stack_ptr = ((uint8_t *)w->ext_stack) + size;
    w->ext_stack = (void *)ext_stack_ptr;
    return ext_stack_ptr;
}

/*
 * All the data needed to properly handle a thrown exception.
 */
struct closure_exception {
    char *exn;
    /* Canonical frame address (CFA) of the call-stack frame from which an
       exception was rethrown.  Used to ensure that the rethrown exception
       appears to be rethrown from the correct frame and to avoid repeated calls
       to __cilkrts_leave_frame during stack unwinding. */
    char *reraise_cfa;
    /* Stack pointer for the parent fiber.  Used to restore the stack pointer
       properly after entering a landingpad. */
    char *parent_rsp;
    /* Fiber holding the stack frame of a call to _Unwind_RaiseException that is
       currently running. */
    struct cilk_fiber *throwing_fiber;
};

//===============================================
// Loop frames
//===============================================

struct __cilkrts_loop_frame {
    // This needs to be on top so that we can just convert the pointer to a
    // __cilkrts_stack_frame
    __cilkrts_stack_frame sf;

    // The indices for our iterations
    uint64_t start;
    uint64_t end;
};

struct __cilkrts_inner_loop_frame {
    // This needs to be on top so that we can just convert the pointer to a
    // __cilkrts_stack_frame
    __cilkrts_stack_frame sf;

    // Because we keep entering and leaving the inner loop frame,
    //  we store a reference to the parent here.
    WHEN_CILK_DEBUG(struct __cilkrts_loop_frame *parentLF);
    // perhaps the multi-D etc
};

/* Is this a __cilkrts_loop_frame ? */
#define CILK_FRAME_LOOP 0x1000u

/* Is this a __cilkrts_inner_loop_frame ? */
#define CILK_FRAME_INNER_LOOP 0x2000u

/* Was a split performed on this frame ? */
#define CILK_FRAME_SPLIT 0x4000u

/* Was this frame allocated dynamically? */
#define CILK_FRAME_DYNAMIC 0x8000u

/* Returns nonzero if the frame is a loop frame. */
static inline unsigned int __cilkrts_is_loop(__cilkrts_stack_frame *sf) {
    return (sf->flags & CILK_FRAME_LOOP);
}

/* Returns nonzero if the frame is an inner loop frame. */
static inline unsigned int __cilkrts_is_inner_loop(__cilkrts_stack_frame *sf) {
    return (sf->flags & CILK_FRAME_INNER_LOOP);
}

/* Returns nonzero if the frame was split. */
static inline unsigned int __cilkrts_is_split(__cilkrts_stack_frame *sf) {
    return (sf->flags & CILK_FRAME_SPLIT);
}

static inline void __cilkrts_set_split(__cilkrts_loop_frame *lf) {
    lf->sf.flags |= CILK_FRAME_SPLIT;
}

// should only be used when copying frames. A frame cannot be "unsplit"
static inline void __cilkrts_set_nonsplit(__cilkrts_loop_frame *lf) {
    lf->sf.flags &= ~CILK_FRAME_SPLIT;
}

/* Returns nonzero if the frame was allocated dynamically (not the original
 * frame). */
static inline unsigned int __cilkrts_is_dynamic(__cilkrts_stack_frame *sf) {
    return (sf->flags & CILK_FRAME_DYNAMIC);
}

static inline void __cilkrts_set_dynamic(__cilkrts_loop_frame *lf) {
    lf->sf.flags |= CILK_FRAME_DYNAMIC;
}

#ifdef __cplusplus
}
#endif

#endif // _CILK_INTERNAL_H
