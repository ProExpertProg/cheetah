set(CHEETAH_LIB_CMAKEFILES_DIR "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}"  PARENT_SCOPE)

# Get sources
set(CHEETAH_SOURCES
  cilk2c.c
  closure.c
  cmdline.c
  debug.c
  fiber.c
  fiber-pool.c
  init.c
  internal-malloc.c
  invoke-main.c
  mutex.c
  loop-frames.c
  readydeque.c
  sched_stats.c
  scheduler.c
  ZERO.c
)

add_library_flags_if(CHEETAH_HAS_PTHREAD_LIB pthread)
add_library_flags_if(CHEETAH_HAS_RT_LIB rt)
if (CHEETAH_USE_COMPILER_RT)
  find_compiler_rt_library(builtins CHEETAH_BUILTINS_LIBRARY)
  add_library_flags_if(CHEETAH_BUILTINS_LIBRARY "${CHEETAH_BUILTINS_LIBRARY}")
else()
  add_library_flags_if(CHEETAH_HAS_GCC_S_LIB gcc_s)
endif()

add_flags_if_supported(-fPIC)


# Build the shared library.
if (CHEETAH_ENABLE_SHARED)
  add_library(cheetah_shared SHARED ${CHEETAH_SOURCES})
  target_link_libraries(cheetah_shared ${CHEETAH_LIBRARIES})
  set_target_properties(cheetah_shared
    PROPERTIES
      LINK_FLAGS    "${CHEETAH_LINK_FLAGS}"
      OUTPUT_NAME   "cheetah"
      VERSION       "${CHEETAH_ABI_VERSION}.0"
      SOVERSION     "${CHEETAH_ABI_VERSION}"
  )
  list(APPEND CHEETAH_BUILD_TARGETS "cheetah_shared")
  if (CHEETAH_INSTALL_SHARED_LIBRARY)
    list(APPEND CHEETAH_INSTALL_TARGETS "cheetah_shared")
  endif()
endif()

# Build the static library.
if (CHEETAH_ENABLE_STATIC)
  add_library(cheetah_static STATIC ${CHEETAH_SOURCES})
  target_link_libraries(cheetah_static ${CHEETAH_LIBRARIES})
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
  set_target_properties(cheetah_static
    PROPERTIES
      LINK_FLAGS    "${CHEETAH_LINK_FLAGS}"
      OUTPUT_NAME   "cheetah"
  )
  list(APPEND CHEETAH_BUILD_TARGETS "cheetah_static")
  if (CHEETAH_INSTALL_STATIC_LIBRARY)
    list(APPEND CHEETAH_INSTALL_TARGETS "cheetah_static")
  endif()
endif()

if (CHEETAH_INSTALL_LIBRARY)
  install(TARGETS ${CHEETAH_INSTALL_TARGETS}
    LIBRARY DESTINATION ${CHEETAH_INSTALL_PREFIX}lib${CHEETAH_LIBDIR_SUFFIX} COMPONENT cheetah
    ARCHIVE DESTINATION ${CHEETAH_INSTALL_PREFIX}lib${CHEETAH_LIBDIR_SUFFIX} COMPONENT cheetah
    )
endif()

# Add a meta-target for both libraries.
add_custom_target(cheetah DEPENDS ${CHEETAH_BUILD_TARGETS})

if (NOT CMAKE_CONFIGURATION_TYPES AND (CHEETAH_INSTALL_LIBRARY OR
                                       CHEETAH_INSTALL_HEADERS))
    if(CHEETAH_INSTALL_LIBRARY)
      set(lib_install_target cheetah)
    endif()
    if(CHEETAH_INSTALL_HEADERS)
      set(header_install_target install-cheetah-headers)
    endif()
    add_custom_target(install-cheetah
                      DEPENDS ${lib_install_target}
                              ${header_install_target}
                      COMMAND "${CMAKE_COMMAND}"
                      -DCMAKE_INSTALL_COMPONENT=cheetah
                      -P "${CHEETAH_BINARY_DIR}/cmake_install.cmake")
    add_custom_target(install-cheetah-stripped
                      DEPENDS ${lib_install_target}
                              ${header_install_target}
                      COMMAND "${CMAKE_COMMAND}"
                      -DCMAKE_INSTALL_COMPONENT=cheetah
                      -DCMAKE_INSTALL_DO_STRIP=1
                      -P "${CHEETAH_BINARY_DIR}/cmake_install.cmake")
endif()
