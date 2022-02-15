//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include <stdint.h>
#include "cilk2c.h"

void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, int64_t count, unsigned int grain) {
    // Try just serial loop
    // printf("Serial cilk_for\n");
    body(data, 0, count);
}

void __cilkrts_cilk_for_32(__cilk_abi_f32_t body, void *data, int32_t count, unsigned int grain) {
    body(data, 0, count);
}
