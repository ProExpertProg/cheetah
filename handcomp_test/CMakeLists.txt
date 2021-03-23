get_directory_property(hasParent PARENT_DIRECTORY)
if (NOT hasParent)
    message(FATAL_ERROR "Cannot build handcomp_test standalone.
    Please build the parent directory with CHEETAH_INCLUDE_TESTS=ON")
endif ()

#===============================================================================
# Common settings and options
#===============================================================================
set(UTIL getoptions.c ktiming.c ZERO.c)

if (NOT DEFINED TIMING_COUNT)
    set(TIMING_COUNT 1)
endif()

add_compile_definitions(TIMING_COUNT=${TIMING_COUNT})

add_compile_options(-Wall -fno-omit-frame-pointer ${HANDCOMP_COMPILE_OPT})

set(LIBRARIES cheetah_static rt pthread m)

#===============================================================================
# Executables
#===============================================================================

#----------------
# Spawn-only
#----------------
add_executable(fib fib.c ${UTIL})
target_link_libraries(fib ${LIBRARIES})

add_executable(mm_dac mm_dac.c ${UTIL})
target_link_libraries(mm_dac ${LIBRARIES})

add_executable(nqueens nqueens.c ${UTIL})
target_link_libraries(nqueens ${LIBRARIES})

add_executable(cilksort cilksort.c ${UTIL})
target_link_libraries(cilksort ${LIBRARIES})

if (NOT DEFINED MANYPROC)
    set(MANYPROC 8)
endif ()


set(COMMAND_NWORKERS ${CMAKE_COMMAND} -E env CILK_NWORKERS=${MANYPROC})
add_custom_target(handcomp_check
        echo \"fib\(26\)\"
        COMMAND ${COMMAND_NWORKERS} $<TARGET_FILE:fib> 26
        COMMAND echo \"mm_dac\(512\)\"
        COMMAND ${COMMAND_NWORKERS} $<TARGET_FILE:mm_dac> -n 512 -c
        COMMAND echo \"nqueens\(10\)\"
        COMMAND ${COMMAND_NWORKERS} $<TARGET_FILE:nqueens> 10
        COMMAND echo \"cilksort\(3000000\)\"
        COMMAND ${COMMAND_NWORKERS} $<TARGET_FILE:cilksort> -n 3000000 -c
        COMMENT "Checking all targets"
        DEPENDS fib mm_dac nqueens cilksort)