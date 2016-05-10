project(rlz CXX C)
set(PROJECT_VENDOR "Andrea Farruggia")
set(PROJECT_CONTACT "a.farruggia@di.unipi.it")
set(PROJECT_DESCRIPTION "RLZAP - Relative Lempel-Ziv with Adaptive Pointers")

cmake_minimum_required(VERSION 2.8.7)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

option(RLZ_BINARIES        "Build binaries"                   ON)
option(RLZ_BENCHMARK       "Build the benchmark binary"       OFF)
option(RLZ_TESTS           "Build tests"                      OFF)

# Turn libdivsufsort64 option ON
option(BUILD_DIVSUFSORT64  "Enable divsufsort64"              ON)

# C++11 support, Release by default
include(Cpp11)
include(NinjaColorFix)
include(ReleaseDefault)
include(submodules)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS_FASTDEB "-g -Wall -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -funroll-loops -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_LTO "-O4 -march=native -ffast-math -funroll-loops -DNDEBUG")
set(CMAKE_CXX_FLAGS_TEST "-Wall -O3 -march=native -ffast-math -funroll-loops -fsanitize=address -DNDEBUG")

include_directories("${PROJECT_SOURCE_DIR}/include/rlz")
list(APPEND RLZ_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")

# SAIS
add_subdirectory("ext_libs/sais")
include_directories("${SAIS_INCLUDE}")
list(APPEND RLZ_INCLUDE_DIRS "${SAIS_INCLUDE}")

# SDSL
add_subdirectory("ext_libs/sdsl")
set(SDSL_INCLUDE "${sdsl_BINARY_DIR}/include")
list(APPEND SDSL_INCLUDE "${sdsl_BINARY_DIR}/external/libdivsufsort/include")
set(SDSL_LIB "sdsl")
list(APPEND SDSL_LIB "divsufsort")
list(APPEND SDSL_LIB "divsufsort64")
include_directories("${SDSL_INCLUDE}")
list(APPEND RLZ_INCLUDE_DIRS "${SDSL_INCLUDE}")

# Boost
add_subdirectory("ext_libs/boost")
include_directories("${BOOST_INCLUDE_DIR}")  
set(Boost_LIBRARIES "boost")
list(APPEND RLZ_INCLUDE_DIRS "${BOOST_INCLUDE_DIR}")

# PAPI
if (RLZ_BENCHMARK)
	find_package(PAPI REQUIRED)
	include_directories(${PAPI_INCLUDE_DIRS})
endif(RLZ_BENCHMARK)

# Libraries
file(GLOB libraries libs/*.c*)
add_library(rlz_lib ${libraries})

if (RLZ_BINARIES)
  # Main executables
  function(exec_add binary)
    add_executable(${binary} ${binary}.cpp)
    target_link_libraries(${binary} rlz_lib sais ${SDSL_LIB} ${Boost_LIBRARIES} ${ARGN})
  endfunction()

	if (RLZ_BENCHMARK)
  	exec_add(benchmark ${PAPI_LIBRARIES})
	endif(RLZ_BENCHMARK)
  exec_add(index_check)
  exec_add(index_decompress)
  exec_add(index_extract)
  exec_add(index_stats)
  exec_add(ms_dump)
  exec_add(rlz_build)
  exec_add(rlzap_build)
  exec_add(space_breakdown)
endif(RLZ_BINARIES)

# Testing
if (RLZ_TESTS)
  enable_testing()
  add_subdirectory("tests")
endif(RLZ_TESTS)

# Export variables
set(RLZ_INCLUDE_DIRS ${RLZ_INCLUDE_DIRS} PARENT_SCOPE)
list(APPEND RLZ_LIBRARIES "rlz_lib")
list(APPEND RLZ_LIBRARIES "sais")
set(RLZ_LIBRARIES ${RLZ_LIBRARIES} PARENT_SCOPE)
