include ../config.mk

HDRS = $(wildcard *.h)

DEFINES = $(ABI_DEF)

# Spawn-only tests
SPAWN_TESTS = cilksort fib mm_dac nqueens
# Tests using the cilk_for ABI (loop frames)
LOOP_TESTS = daxpy nqueens_loop mandelbrot mtrans mmul_loop mm_dac_loop loop_spawn
# Serial baselines, not linked with the runtime
SERIAL_TESTS = mandelbrot_serial

TESTS = $(SPAWN_TESTS) $(LOOP_TESTS) $(SERIAL_TESTS)
UTIL = ktiming.o getoptions.o ZERO.o

INCLUDES = -I../include/
OPTIONS = $(OPT) $(ARCH) $(DBG) -Wall $(DEFINES) $(INCLUDES) -fno-omit-frame-pointer
# dynamic linking
# RTS_DLIBS = -L../runtime -Wl,-rpath -Wl,../runtime -lopencilk
# RTS_LIBS = ../runtime/$(RTS_LIB).so
# static linking
RTS_LIBS = $(RTS_LIBDIR)/$(RTS_LIB).a
TIMING_COUNT ?= 1

.PHONY: all check memcheck clean

all: $(TESTS)

# Each test links its own object plus the utilities. Some tests have extra
# objects (listed below).
$(SPAWN_TESTS) $(LOOP_TESTS): %: %.o $(UTIL)
	$(CC) $(filter %.o,$^) -o $@ $(RTS_LIBS) $(EXTRA_LDFLAGS) -lrt -lpthread -lm

# mm_dac_loop both inlines the ABI (it spawns) and calls cilk_for, so the
# inlined functions are defined in the test and in the runtime's cilk_for object.
mm_dac_loop loop_spawn: EXTRA_LDFLAGS = -Wl,--allow-multiple-definition

# mandelbrot compiled without Cilk, as the serial baseline
mandelbrot_serial: mandelbrot_serial.o $(UTIL)
	$(CC) $(filter %.o,$^) -o $@ -lrt -lpthread -lm

mandelbrot_serial.o: mandelbrot.c $(HDRS)
	$(CC) -c $(OPTIONS) -DTIMING_COUNT=$(TIMING_COUNT) -DSERIAL=1 -Dcilk_main=main -o $@ $<

%.o: %.c $(HDRS)
	$(CC) -c $(OPTIONS) -DTIMING_COUNT=$(TIMING_COUNT) -o $@ $<

memcheck: 
	$(MAKE) clean; $(MAKE) > /dev/null
	date
	CILK_NWORKERS=8 valgrind ./fib 26
	CILK_NWORKERS=8 valgrind ./mm_dac -n 512
	CILK_NWORKERS=8 valgrind ./cilksort -n 3000000
	CILK_NWORKERS=8 valgrind ./nqueens 10
	CILK_NWORKERS=8 valgrind ./daxpy -n 100000 -c
	CILK_NWORKERS=8 valgrind ./nqueens_loop 10
	CILK_NWORKERS=8 valgrind ./mandelbrot -c
	CILK_NWORKERS=8 valgrind ./mtrans -n 512 -c
	CILK_NWORKERS=8 valgrind ./mmul_loop -n 128 -c
	CILK_NWORKERS=8 valgrind ./mm_dac_loop -n 256 -c
	date

check:
	$(MAKE) clean; $(MAKE) TIMING_COUNT=5 > /dev/null
	CILK_NWORKERS=$(MANYPROC) ./fib 40
	CILK_NWORKERS=$(MANYPROC) ./mm_dac -n 1024 -c
	CILK_NWORKERS=$(MANYPROC) ./cilksort -n 30000000 -c
	CILK_NWORKERS=$(MANYPROC) ./nqueens 14
	CILK_NWORKERS=$(MANYPROC) ./daxpy -n 10000000 -c
	CILK_NWORKERS=$(MANYPROC) ./nqueens_loop 14
	CILK_NWORKERS=$(MANYPROC) ./mandelbrot -c
	CILK_NWORKERS=$(MANYPROC) ./mtrans -n 2048 -c
	CILK_NWORKERS=$(MANYPROC) ./mmul_loop -n 512 -c
	CILK_NWORKERS=$(MANYPROC) ./mm_dac_loop -n 1024 -c

clean:
	rm -f *.o *~ $(TESTS) core.*
