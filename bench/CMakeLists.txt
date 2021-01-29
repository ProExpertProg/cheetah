get_directory_property(hasParent PARENT_DIRECTORY)
if (NOT hasParent)
    message(FATAL_ERROR "Cannot build bench standalone.\nPlease build the parent directory with CHEETAH_INCLUDE_TESTS=ON")
endif ()

#===============================================================================
# Common settings and options
#===============================================================================
set(UTIL getoptions.c ktiming.c)

if (NOT DEFINED TIMING_COUNT)
    set(TIMING_COUNT 10)
endif()

add_compile_definitions(TIMING_COUNT=${TIMING_COUNT})

add_compile_options(
        -fopencilk -fno-exceptions -ftapir=cheetah
        -fno-omit-frame-pointer
        "SHELL:-mllvm -cilk-use-runtime-cilkfor" # prevent de-duplication
        "SHELL:-mllvm -use-external-abi-functions" # prevent de-duplication
        "SHELL:-mllvm -stripmine-coarsen-factor=1" # prevent de-duplication
        #"SHELL:-mllvm -default-runtime-grainsize=256" # prevent de-duplication
        -Rpass=loop-stripmine
        ${BENCH_COMPILE_OPT})

set(LIBRARIES cheetah_static rt pthread m dl)

#===============================================================================
# Executables
#===============================================================================

add_executable(cholesky_bench cholesky.c ${UTIL})
target_link_libraries(cholesky_bench ${LIBRARIES})

add_executable(cilksort_bench cilksort.c ${UTIL})
target_link_libraries(cilksort_bench ${LIBRARIES})

add_executable(fft_bench fft.c ${UTIL})
target_link_libraries(fft_bench ${LIBRARIES})

add_executable(fib_bench fib.c ${UTIL})
target_link_libraries(fib_bench ${LIBRARIES})

add_executable(heat_bench heat.c ${UTIL})
target_link_libraries(heat_bench ${LIBRARIES})

add_executable(heat-loop_bench heat-loop.c ${UTIL})
target_link_libraries(heat-loop_bench ${LIBRARIES})

add_executable(lu_bench lu.c ${UTIL})
target_link_libraries(lu_bench ${LIBRARIES})

add_executable(matmul_4_way_z_bench matmul_4_way_z.c ${UTIL})
target_link_libraries(matmul_4_way_z_bench ${LIBRARIES})

add_executable(mm_dac_bench mm_dac.c ${UTIL})
target_link_libraries(mm_dac_bench ${LIBRARIES})

add_executable(nqueens_bench nqueens.c ${UTIL})
target_link_libraries(nqueens_bench ${LIBRARIES})

add_executable(strassen_bench strassen.c ${UTIL})
target_link_libraries(strassen_bench ${LIBRARIES})

add_executable(mandelbrot_bench mandelbrot.c ${UTIL})
target_link_libraries(mandelbrot_bench ${LIBRARIES})

add_executable(daxpy_bench daxpy.c ${UTIL})
target_link_libraries(daxpy_bench ${LIBRARIES})