//
// Created by Luka on 1/11/2021.
// Taken from the CilkPlus benchmark suite
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "cilk_for.h"
#include "getoptions.h"
#include "ktiming.h"

#ifndef TIMING_COUNT
#define TIMING_COUNT 1
#endif

struct innerData {
    double x0, y0, xstep, ystep;
    int width, max_depth;
    unsigned char *output;
    int64_t j; // also grainsize for loop
};

void innerBody(int64_t i, void *data) {
    struct innerData *d = data;

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
#ifndef SERIAL
void outerBody(int64_t j, void *data) {
    struct innerData *d = data;
    struct innerData id = *d;
    id.j = j;
    cilk_for(id.width, &id, innerBody, d->j);
}
#endif

unsigned char *
mandelbrot(double x0, double y0, double x1, double y1, int width, int height, int max_depth, unsigned char *output,
           uint64_t grainsize) {
    double xstep = (x1 - x0) / width;
    double ystep = (y1 - y0) / height;

    struct innerData outerD = {
            .x0=x0, .y0=y0, .xstep=xstep, .ystep=ystep,
            .width=width, .max_depth=max_depth, .output=output, .j=grainsize
    };

    // Traverse the sample space in equally spaced steps with width * height samples
#ifdef SERIAL
    for (int j = 0; j < height; j++) {
        outerD.j = j;
        for (int i = 0; i < width; i++) {
            innerBody(i, &outerD);
        }
    }
#else
    cilk_for(height, &outerD, outerBody, 1);
#endif
    return output;
}


int usage(void) {
    fprintf(stderr,
            "\nUsage: mandelbrot [-x width] [-x height] [-d depth] [-g grainsize] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-x", "-y", "-d", "-c", "-g", "-h", 0};
int opt_types[] = {LONGARG, LONGARG, INTARG, BOOLARG, LONGARG, BOOLARG, 0};

int main(int argc, char *argv[]) {

    double x0 = -2.5;
    double y0 = -0.875;
    double x1 = 1;
    double y1 = 0.875;
    int height = 1024, width = 2048;

    int max_depth = 100;
    unsigned int grainsize = 1;

    int help = 0, check = 0;

    get_options(argc, argv, specifiers, opt_types, &width, &height, &max_depth, &check, &grainsize, &help);

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
        mandelbrot(x0, y0, x1, y1, width, height, max_depth, output, grainsize);
        end = ktiming_getmark();
        running_time[t] = ktiming_diff_nsec(&begin, &end);
        printf("Output: %x\n", output[3]); // otherwise the whole mandelbrot function gets optimized away in the case of the serial loop
    }

    print_runtime(running_time, TIMING_COUNT);

    free(output);
    return 0;
}
