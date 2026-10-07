//
// Created by Luka on 2/17/2022.
//

#ifndef CHEETAH_CILK_ABI_CILK_FOR_H
#define CHEETAH_CILK_ABI_CILK_FOR_H

#include <alloca.h>
#include <stddef.h>

#include "cilk2c.h"

// Hand-written spawners must contain a dynamic alloca, so the compiler sees a
// function with variable-sized stack objects (the same effect as the
// reserves an outgoing-argument area addressed relative to SP, and stack
// `stealable` attribute that the Cilk compiler puts on spawners).  Otherwise it
// arguments (7th+ argument of a call) get stored at 0(%rsp) -- which, after the
// runtime moves SP to the top of the fiber, is the fiber header.
static volatile size_t zero_for_alloca = 0;
static void __attribute__((noinline)) dummy_use(void *p) { __asm__ volatile("" ::"r"(p) : "memory"); }
#define FORCE_VAR_SIZED_FRAME() dummy_use(alloca(zero_for_alloca))

// Decided to implement runtime calls in the header file in order to avoid repetition and enable inlining.
// If __cilkrts_cilk_for_64 and cilk_for_impl_64 weren't in the same translation unit, they couldn't get inlined (not supporting -lto yet)
// Implementations are static void to make sure that they aren't visible anywhere else.
static void cilk_for_impl_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain, int inclusive);
static void cilk_for_impl_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain, int inclusive);

void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain) {
    cilk_for_impl_64(body, data, count, grain, 0);
}

void __cilkrts_cilk_for_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain) {
    cilk_for_impl_32(body, data, count, grain, 0);
}

void __cilkrts_cilk_for_inclusive_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain) {
    cilk_for_impl_64(body, data, count, grain, 1);
}

void __cilkrts_cilk_for_inclusive_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain) {
    cilk_for_impl_32(body, data, count, grain, 1);
}

#endif //CHEETAH_CILK_ABI_CILK_FOR_H
