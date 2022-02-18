//
// Created by Luka on 2/17/2022.
//

#ifndef CHEETAH_CILK_ABI_CILK_FOR_H
#define CHEETAH_CILK_ABI_CILK_FOR_H

#include "cilk2c.h"

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
