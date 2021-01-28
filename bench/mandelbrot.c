//
// Created by Luka on 1/20/2021.
//

#include <cilk/cilk.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "getoptions.h"
#include "ktiming.h"

// Description:
// Determines how deeply points in the complex plane, spaced on a uniform grid, remain in the Mandelbrot set.
// The uniform grid is specified by the rectangle (x1, y1) - (x0, y0).
// Mandelbrot set is determined by remaining bounded after iteration of z_n+1 = z_n^2 + c, up to max_depth.
//
// iterating through the complex plane is accomplished with cilk_for
//
// [in]: x0, y0, x1, y1, width, height, max_depth
// [out]: output (caller must deallocate)
__attribute__ ((__noinline__))
unsigned char *cilk_mandelbrot(double x0, double y0, double x1, double y1, int width, int height, int max_depth,
                               unsigned char *output) {
    double xstep = (x1 - x0) / width;
    double ystep = (y1 - y0) / height;

    // Traverse the sample space in equally spaced steps with width * height samples
    cilk_for(int j = 0; j < height; ++j) {
        cilk_for (int i = 0; i < width; ++i) {
            double z_real = x0 + i*xstep;
            double z_imaginary = y0 + j*ystep;
            double c_real = z_real;
            double c_imaginary = z_imaginary;

            // depth should be an int, but the vectorizer will not vectorize, complaining about mixed data types
            // switching it to double is worth the small cost in performance to let the vectorizer work
            double depth = 0;
            // Figures out how many recurrences are required before divergence, up to max_depth
            while(depth < max_depth) {
                if(z_real * z_real + z_imaginary * z_imaginary > 4.0) {
                    break; // Escape from a circle of radius 2
                }
                double temp_real = z_real*z_real - z_imaginary*z_imaginary;
                double temp_imaginary = 2.0*z_real*z_imaginary;
                z_real = c_real + temp_real;
                z_imaginary = c_imaginary + temp_imaginary;

                ++depth;
            }
            output[j*width + i] = (unsigned char) ((depth / max_depth) * 255);
        }
    }
    return output;
}


int usage(void) {
    fprintf(stderr,
            "\nUsage: mandelbrot [-n height] [-g grainsize] [-c] [-h]\n\n");
    return -1;
}

const char *specifiers[] = {"-n", "-c", "-g", "-h", 0};
int opt_types[] = {LONGARG, BOOLARG, LONGARG, BOOLARG, 0};

int main(int argc, char *argv[]) {

    double x0 = -2.5;
    double y0 = -0.875;
    double x1 = 1;
    double y1 = 0.875;
    uint64_t height = 4096, width;

    int max_depth = 10;
    uint64_t grainsize = 1;

    int help = 0, check = 0;

    get_options(argc, argv, specifiers, opt_types, &height, &check, &grainsize, &help);

    if (help) {
        return usage();
    }

    width = 2 * height;
    // Width should be a multiple of 8
    assert(width % 8 == 0);

    unsigned char *output = aligned_alloc(64, width * height * sizeof(unsigned char));

    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    for (int t = 0; t < TIMING_COUNT; t++) {
        begin = ktiming_getmark();
        cilk_mandelbrot(x0, y0, x1, y1, width, height, max_depth, output);
        end = ktiming_getmark();
        running_time[t] = ktiming_diff_usec(&begin, &end);
        printf("Output: %x\n", output[3]); // otherwise the whole mandelbrot function gets optimized away in the case of the serial loop
    }

    print_runtime(running_time, TIMING_COUNT);

    free(output);
    return 0;
}
