#ifndef _RSCHED_H
#define _RSCHED_H

#include "worker.h"
#include "closure.h"

void longjmp_to_runtime(__cilkrts_worker * w);

Closure *setup_for_execution(__cilkrts_worker * ws, Closure *t);

Closure *do_what_it_says(__cilkrts_worker * ws, Closure *t);

void worker_scheduler(__cilkrts_worker * w, Closure * t);

#endif
