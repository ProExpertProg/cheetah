//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include <stdint.h>
#include "cilk_abi_cilk_for.h"

// CMake counts on the for_loop file to pull in the inlined functions
#include "cilk2c_inlined.c"

static void cilk_for_impl_64(__cilk_abi_f64_t body, void *data, uint64_t count, unsigned int grain, int inclusive) {
    body(data, 0, count);
}

static void cilk_for_impl_32(__cilk_abi_f32_t body, void *data, uint32_t count, unsigned int grain, int inclusive) {
    body(data, 0, count);
}
