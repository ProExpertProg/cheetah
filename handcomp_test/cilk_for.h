//
// Created by Luka on 11/10/2020.
//

#ifndef CHEETAH_CILK_FOR_H
#define CHEETAH_CILK_FOR_H

#include <stdint.h>

typedef void (*ForBody)(uint64_t i, void *data);

void cilk_for(uint64_t low, uint64_t high, void *data, ForBody body, uint64_t grainsize);

#endif //CHEETAH_CILK_FOR_H
