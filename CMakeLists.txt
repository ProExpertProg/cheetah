# CMake build for Cheetah.

cmake_minimum_required(VERSION 3.4.3)

if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
  set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
endif()

# Add path for custom cheetah modules.
set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules"
  ${CMAKE_MODULE_PATH}
  )

# Check if cheetah is built as a standalone project.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR CHEETAH_STANDALONE_BUILD)
  project(Cheetah CXX C)

  set(PACKAGE_NAME cheetah)
  set(PACKAGE_VERSION 8.0.1)
  set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
  set(PACKAGE_BUGREPORT "angelee@wustl.edu")

  # Find the LLVM sources and simulate LLVM CMake options.
  include(HandleOutOfTreeLLVM)
endif()

# Require out of source build.
include(MacroEnsureOutOfSourceBuild)
MACRO_ENSURE_OUT_OF_SOURCE_BUILD(
  "${PROJECT_NAME} requires an out of source build. Please create a separate
 build directory and run 'cmake /path/to/${PROJECT_NAME} [options]' there."
  )

#===============================================================================
# Setup CMake Options
#===============================================================================
include(CMakeDependentOption)
include(HandleCompilerRT)

# Basic options ---------------------------------------------------------------
option(CHEETAH_ENABLE_ASSERTIONS "Enable assertions independent of build mode." OFF)
option(CHEETAH_ENABLE_WERROR "Fail and stop if a warning is triggered." OFF)
option(CHEETAH_USE_COMPILER_RT "Use compiler-rt instead of libgcc" OFF)

option(CHEETAH_INCLUDE_TESTS "Generate build targets for the cheetah unit tests." ${LLVM_INCLUDE_TESTS})
set(CHEETAH_LIBDIR_SUFFIX "${LLVM_LIBDIR_SUFFIX}" CACHE STRING
    "Define suffix of library directory name (32/64)")
option(CHEETAH_INSTALL_LIBRARY "Install the cheetah library." ON)
set(CHEETAH_TARGET_TRIPLE "" CACHE STRING "Target triple for cross compiling.")
set(CHEETAH_GCC_TOOLCHAIN "" CACHE PATH "GCC toolchain for cross compiling.")
set(CHEETAH_SYSROOT "" CACHE PATH "Sysroot for cross compiling.")

option(CHEETAH_ENABLE_SHARED "Build cheetah as a shared library." ON)
option(CHEETAH_ENABLE_STATIC "Build cheetah as a static library." ON)

cmake_dependent_option(CHEETAH_INSTALL_STATIC_LIBRARY
  "Install the static cheetah library." ON
  "CHEETAH_ENABLE_STATIC;CHEETAH_INSTALL_LIBRARY" OFF)
cmake_dependent_option(CHEETAH_INSTALL_SHARED_LIBRARY
  "Install the shared cheetah library." ON
  "CHEETAH_ENABLE_SHARED;CHEETAH_INSTALL_LIBRARY" OFF)

set(CHEETAH_ABI_VERSION "1" CACHE STRING "ABI version of cheetah. Defaults to 1.")

if (NOT CHEETAH_ENABLE_SHARED AND NOT CHEETAH_ENABLE_STATIC)
  message(FATAL_ERROR "cheetah must be built as either a shared or static library.")
endif()

# Target options --------------------------------------------------------------
set(CHEETAH_SYSROOT "" CACHE STRING "Use alternate sysroot.")
set(CHEETAH_GCC_TOOLCHAIN "" CACHE STRING "Use alternate GCC toolchain.")

#===============================================================================
# Configure System
#===============================================================================

set(CHEETAH_COMPILER    ${CMAKE_CXX_COMPILER})
set(CHEETAH_SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
set(CHEETAH_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})

string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" CLANG_VERSION
       ${PACKAGE_VERSION})

if(LLVM_ENABLE_PER_TARGET_RUNTIME_DIR AND NOT APPLE)
  set(DEFAULT_INSTALL_PREFIX lib${LLVM_LIBDIR_SUFFIX}/clang/${CLANG_VERSION}/${LLVM_DEFAULT_TARGET_TRIPLE}/)
  set(CHEETAH_LIBRARY_DIR ${LLVM_LIBRARY_OUTPUT_INTDIR}/clang/${CLANG_VERSION}/${LLVM_DEFAULT_TARGET_TRIPLE}/lib${CHEETAH_LIBDIR_SUFFIX})
elseif(LLVM_LIBRARY_OUTPUT_INTDIR)
  set(CHEETAH_LIBRARY_DIR ${LLVM_LIBRARY_OUTPUT_INTDIR})
else()
  set(CHEETAH_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib${CHEETAH_LIBDIR_SUFFIX})
endif()

set(CHEETAH_INSTALL_PREFIX ${DEFAULT_INSTALL_PREFIX} CACHE STRING
    "Define cheetah destination prefix.")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CHEETAH_LIBRARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CHEETAH_LIBRARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CHEETAH_LIBRARY_DIR})

set(CHEETAH_C_FLAGS "")
set(CHEETAH_CXX_FLAGS "")
set(CHEETAH_COMPILE_FLAGS "")
set(CHEETAH_LINK_FLAGS "")
set(CHEETAH_LIBRARIES "")

# Include macros for adding and removing cheetah flags.
include(HandleCheetahFlags)

#===============================================================================
# Setup Compiler Flags
#===============================================================================

# Configure target flags
add_target_flags_if(CHEETAH_TARGET_TRIPLE
          "--target=${CHEETAH_TARGET_TRIPLE}")
add_target_flags_if(CHEETAH_GCC_TOOLCHAIN
         "--gcc-toolchain=${CHEETAH_GCC_TOOLCHAIN}")
add_target_flags_if(CHEETAH_SYSROOT
          "--sysroot=${CHEETAH_SYSROOT}")

if (CHEETAH_TARGET_TRIPLE)
  set(TARGET_TRIPLE "${CHEETAH_TARGET_TRIPLE}")
endif()

# Configure compiler.
include(config-ix)

if (CHEETAH_USE_COMPILER_RT)
  list(APPEND CHEETAH_LINK_FLAGS "--rtlib=compiler-rt")
endif()

# Get warning flags
add_compile_flags_if_supported(-Wall)

if (CHEETAH_ENABLE_WERROR)
  add_compile_flags_if_supported(-Werror)
else()
  add_compile_flags_if_supported(-Wno-error)
endif()

# The spawn_main symbol in cheetah is undefined.  This routine
# corresponds to the entry point of the compiled Cilk program.
set(CHEETAH_HAS_UNDEFINED_SYMBOLS ON)

if (CHEETAH_HAS_UNDEFINED_SYMBOLS)
  # Need to allow unresolved symbols if this is to work with shared library builds
  if (APPLE)
    list(APPEND CHEETAH_LINK_FLAGS "-undefined dynamic_lookup")
  else()
    # Relax this restriction from HandleLLVMOptions
    string(REPLACE "-Wl,-z,defs" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
  endif()
endif()

string(REPLACE ";" " " CHEETAH_CXX_FLAGS "${CHEETAH_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CHEETAH_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CHEETAH_C_FLAGS}")

#===============================================================================
# Setup Source Code
#===============================================================================

if(CHEETAH_INCLUDE_TESTS)
  # handcomp tests use the static library
  set(CHEETAH_ENABLE_STATIC ON)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Release)
  add_compile_definitions(CILK_DEBUG=0)
else()
  add_compile_definitions(CILK_DEBUG=1)
endif()

include_directories(runtime)

# Add source code. This also contains all of the logic for deciding linker flags
# soname, etc...
add_subdirectory(runtime)

if (CHEETAH_INCLUDE_TESTS)
  add_subdirectory(handcomp_test)
  add_subdirectory(bench)
endif()
