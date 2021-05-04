get_directory_property(hasParent PARENT_DIRECTORY)
if (NOT hasParent)
    message(FATAL_ERROR "Cannot build reducer_bench standalone.\nPlease build the parent directory with CHEETAH_INCLUDE_TESTS=ON")
endif ()

#===============================================================================
# Common settings and options
#===============================================================================
set(UTIL getoptions.c ktiming.c)

if (NOT DEFINED TIMING_COUNT)
    set(TIMING_COUNT 1)
endif()

add_compile_definitions(TIMING_COUNT=${TIMING_COUNT})

add_compile_options(
        -fopencilk -fno-exceptions # -ftapir=cheetah
        #-fno-omit-frame-pointer
        "SHELL:-mllvm -cilk-use-runtime-cilkfor" # prevent de-duplication
        # "SHELL:-mllvm -use-external-abi-functions" # prevent de-duplication
        # TODO "SHELL:-mllvm -stripmine-coarsen-factor=1" # prevent de-duplication
        #"SHELL:-mllvm -default-runtime-grainsize=256" # prevent de-duplication
        "--opencilk-abi-bitcode=${CHEETAH_ABI_BITCODE_FILE}"
        -Rpass=loop-stripmine
        ${BENCH_COMPILE_OPT})

set(LIBRARIES cheetah_static rt pthread m dl)

#===============================================================================
# Executables
#===============================================================================

add_executable(intsum intsum.c ${UTIL})
target_link_libraries(intsum ${LIBRARIES})
add_dependencies(intsum cheetah_abi_bc)

add_executable(cppsum cppsum.cpp ZERO.c ${UTIL})
target_link_libraries(cppsum ${LIBRARIES})
add_dependencies(intsum cheetah_abi_bc)
