//
// Created by Luka on 1/14/2021.
//

#include <stdio.h>
#include "stdint.h"
#include "cilk2c.h"
#include "scheduler.h"

typedef void (*__cilk_abi_f64_t)(void *data, int64_t low, int64_t high);
void __cilkrts_cilk_for_64(__cilk_abi_f64_t body, void *data, uint64_t count, int grain) {
    // Try just serial loop
    // printf("Serial cilk_for\n");
    body(data, 0, count);
}
