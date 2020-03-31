# <UNTESTED>
get_directory_property(hasParent PARENT_DIRECTORY)
if (NOT hasParent)
    set(CHEETAH_INCLUDE_TESTS FALSE)
    add_directory(..)
endif ()
# </UNTESTED>

#===============================================================================
# Common settings and options
#===============================================================================
set(UTIL getoptions.c ktiming.c)

set(TIMING_COUNT 1)
add_compile_definitions(TIMING_COUNT=${TIMING_COUNT})

add_compile_options(-Wall -fno-omit-frame-pointer ${HANDCOMP_COMPILE_OPT})

set(LIBRARIES cheetah_shared rt pthread m)

#===============================================================================
# Executables
#===============================================================================

add_executable(fib fib.c ${UTIL})
target_link_libraries(fib ${LIBRARIES})

add_executable(mm_dac mm_dac.c ${UTIL})
target_link_libraries(mm_dac ${LIBRARIES})

add_executable(nqueens nqueens.c ${UTIL})
target_link_libraries(nqueens ${LIBRARIES})

add_executable(cilksort cilksort.c ${UTIL})
target_link_libraries(cilksort ${LIBRARIES})