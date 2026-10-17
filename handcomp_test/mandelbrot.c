//
// Created by Luka on 1/11/2021.
// Taken from the CilkPlus benchmark suite
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../runtime/cilk2c.h"
#include "getoptions.h"
#include "ktiming.h"

#ifndef TIMING_COUNT
#define TIMING_COUNT 1
#endif

struct innerData {
    double x0, y0, xstep, ystep;
    int width, max_depth;
    unsigned char *output;
    uint64_t j; // also grainsize for the inner loop
    uint64_t outer_grain; // grainsize of the outer loop (unused in the inner copies)
};

void innerBody(void *data, uint64_t low, uint64_t high) {
    struct innerData *d = data;

    for (uint64_t i = low; i < high; ++i) {
        double z_real = d->x0 + i * d->xstep;
        double z_imaginary = d->y0 + d->j * d->ystep;
        double c_real = z_real;
        double c_imaginary = z_imaginary;

        // depth should be an int, but the vectorizer will not vectorize, complaining about mixed data types
        // switching it to double is worth the small cost in performance to let the vectorizer work
        double depth = 0;
        // Figures out how many recurrences are required before divergence, up to max_depth
        while (depth < d->max_depth) {
            if (z_real * z_real + z_imaginary * z_imaginary > 4.0) {
                break; // Escape from a circle of radius 2
            }
            double temp_real = z_real * z_real - z_imaginary * z_imaginary;
            double temp_imaginary = 2.0 * z_real * z_imaginary;
            z_real = c_real + temp_real;
            z_imaginary = c_imaginary + temp_imaginary;

            ++depth;
        }
        d->output[d->j * d->width + i] = (unsigned char) ((depth / d->max_depth) * 255);
    }
}
#ifndef SERIAL
void outerBody(void *data, uint64_t low, uint64_t high) {
    struct innerData *d = data;
    for (uint64_t j = low; j < high; ++j) {
        struct innerData id = *d;
        id.j = j;
        __cilkrts_cilk_for_64(innerBody, &id, id.width, d->j);
    }
}
#endif

unsigned char *
mandelbrot(double x0, double y0, double x1, double y1, int width, int height, int max_depth, unsigned char *output,
           uint64_t grainsize, uint64_t outer_grain) {
    double xstep = (x1 - x0) / width;
    double ystep = (y1 - y0) / height;

    struct innerData outerD = {
            .x0=x0, .y0=y0, .xstep=xstep, .ystep=ystep,
            .width=width, .max_depth=max_depth, .output=output, .j=grainsize, .outer_grain=outer_grain // reuse j
    };

    // Traverse the sample space in equally spaced steps with width * height samples
#ifdef SERIAL
    for (int j = 0; j < height; j++) {
        outerD.j = j;
        innerBody(&outerD, 0, width);
    }
#else
    __cilkrts_cilk_for_64(outerBody, &outerD, height, outer_grain);
#endif
    return output;
}


int usage(void) {
    fprintf(stderr,
            "\nUsage: mandelbrot [-x width] [-y height] [-n width=height/2] [-d depth] [-g inner grainsize] [-o outer grainsize] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-x", "-y", "-n", "-d", "-c", "-g", "-o", "-h", 0};
int opt_types[] = {LONGARG, LONGARG, LONGARG, INTARG, BOOLARG, LONGARG, LONGARG, BOOLARG, 0};

int main(int argc, char *argv[]) {

    double x0 = -2.5;
    double y0 = -0.875;
    double x1 = 1;
    double y1 = 0.875;
    long height = 1024, width = 2048; // long because LONGARG stores a long
    long n = 0;

    int max_depth = 100;
    long grainsize = 1; // long because LONGARG stores a long
    long outer_grain = 1;

    int help = 0, check = 0;

    get_options(argc, argv, specifiers, opt_types, &width, &height, &n, &max_depth, &check, &grainsize, &outer_grain, &help);

    if (n != 0) {
        height = n;
        width = 2*n;
    }

    if (help) {
        return usage();
    }

    // Width should be a multiple of 8
    assert(width % 8 == 0);

    unsigned char *output = aligned_alloc(64, width * height * sizeof(unsigned char));

    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    for (int t = 0; t < TIMING_COUNT; t++) {
        begin = ktiming_getmark();
        mandelbrot(x0, y0, x1, y1, width, height, max_depth, output, grainsize, outer_grain);
        end = ktiming_getmark();
        running_time[t] = ktiming_diff_nsec(&begin, &end);
        printf("Output: %x\n", output[3]); // otherwise the whole mandelbrot function gets optimized away in the case of the serial loop
    }

    print_runtime(running_time, TIMING_COUNT);

    free(output);
    return 0;
}
