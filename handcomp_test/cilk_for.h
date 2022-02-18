//
// Created by Luka Govedic on 11/17/2021.
//

#ifndef CHEETAH_CILK_FOR_H
#define CHEETAH_CILK_FOR_H

#include <stdint.h>
#include <cilk2c.h>

typedef void (*ForBody)(uint64_t i, void *data);

typedef struct {
    void *innerData;
    ForBody body;
} BodyAdapterData;

void bodyAdapter(void *data, uint64_t low, uint64_t high) {
    const BodyAdapterData *d = data;
    for (uint64_t i = low; i < high; ++i) {
        d->body(i, d->innerData);
    }
}

static inline void cilk_for(uint64_t count, void *data, ForBody body, unsigned int grainsize) {
    BodyAdapterData d;
    d.innerData = data;
    d.body = body;

    __cilkrts_cilk_for_64(bodyAdapter, &d, count, grainsize);
}



#endif //CHEETAH_CILK_FOR_H
