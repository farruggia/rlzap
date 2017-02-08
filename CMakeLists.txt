project(rlz CXX C)
set(PROJECT_VENDOR "Andrea Farruggia")
set(PROJECT_CONTACT "a.farruggia@di.unipi.it")
set(PROJECT_DESCRIPTION "RLZAP - Relative Lempel-Ziv with Adaptive Pointers")

cmake_minimum_required(VERSION 2.8.7)

# Options
option(RLZ_BINARIES             "Build binaries"                           OFF)
option(RLZ_BENCHMARK            "Build the benchmark binary"               OFF)
option(RLZ_TESTS                "Build tests"                              OFF)
option(RLZ_USE_EXTERNAL_SDSL    "Uses an external SDSL installation"       OFF)
option(RLZ_USE_EXTERNAL_BOOST   "Uses an external Boost installation"       ON)
option(BUILD_DIVSUFSORT64       "Enable divsufsort64"                       ON)

# Useful modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
include(Cpp11)
include(NinjaColorFix)
include(ReleaseDefault)
include(submodules)

# Compilation flags (RELEASE by default)
set(CMAKE_CXX_FLAGS_FASTDEB "-g -Wall -O3 -ftemplate-depth=2048")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -O0 -ftemplate-depth=2048")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -funroll-loops -DNDEBUG -ftemplate-depth=2048")
set(CMAKE_CXX_FLAGS_RELEASE_LTO "-O4 -march=native -ffast-math -funroll-loops -DNDEBUG -ftemplate-depth=2048")
set(CMAKE_CXX_FLAGS_TEST "-Wall -O3 -march=native -ffast-math -funroll-loops -fsanitize=address -DNDEBUG -ftemplate-depth=2048")

# Main library include directories
set(RLZ_LIBRARY_DIRS "")
include_directories("${PROJECT_SOURCE_DIR}/include/rlz")
list(APPEND RLZ_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")

# This piece of code sets SDSL_INCLUDE_DIRS and SDSL_LIBRARIES accordingly
if (NOT SDSL_INCLUDE_DIRS)
  if (RLZ_USE_EXTERNAL_SDSL)
    include(FindPkgConfig)
    pkg_search_module(SDSL REQUIRED sdsl)
  elseif (NOT TARGET sdsl)
    add_subdirectory("ext_libs/sdsl")
    set(SDSL_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ext_libs/sdsl")
    list(APPEND SDSL_INCLUDE_DIRS "${SDSL_BINARY_DIR}/include")
    list(APPEND SDSL_INCLUDE_DIRS "${sdsl_BINARY_DIR}/external/libdivsufsort/include")
    
    list(APPEND SDSL_LIBRARIES "sdsl")
    list(APPEND SDSL_LIBRARIES "divsufsort")
    list(APPEND SDSL_LIBRARIES "divsufsort64")
  else()
    message(FATAL_ERROR "A sdsl target has been found but needed variable SDSL_INCLUDE_DIRS is not set.")
  endif(RLZ_USE_EXTERNAL_SDSL)
endif(NOT SDSL_INCLUDE_DIRS)

include_directories("${SDSL_INCLUDE_DIRS}")
list(APPEND RLZ_INCLUDE_DIRS "${SDSL_INCLUDE_DIRS}")

# SAIS
add_subdirectory("ext_libs/sais")
include_directories("${SAIS_INCLUDE}")
list(APPEND RLZ_INCLUDE_DIRS "${SAIS_INCLUDE}")

# Boost
if (RLZ_USE_EXTERNAL_BOOST)
  find_package(Boost 1.46 REQUIRED COMPONENTS program_options)
  link_directories(${Boost_LIBRARY_DIRS})
  list(APPEND RLZ_LIBRARY_DIRS "${Boost_LIBRARY_DIRS}")
else()
  add_subdirectory("ext_libs/boost")
endif(RLZ_USE_EXTERNAL_BOOST)

include_directories(${Boost_INCLUDE_DIR})
list(APPEND RLZ_INCLUDE_DIRS "${Boost_INCLUDE_DIR}")
list(APPEND RLZ_LIBRARIES "${Boost_LIBRARIES}")

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
    target_link_libraries(${binary} rlz_lib sais ${SDSL_LIBRARIES} ${Boost_LIBRARIES} ${ARGN})
  endfunction()

	if (RLZ_BENCHMARK)
  	exec_add(benchmark ${PAPI_LIBRARIES})
	endif(RLZ_BENCHMARK)
  exec_add(index_check)
  exec_add(index_decompress)
  exec_add(index_extract)
  exec_add(index_stats)
  exec_add(ms_dump)
  exec_add(rlzap_build)
  exec_add(space_breakdown)
endif(RLZ_BINARIES)

# Testing
if (RLZ_TESTS)
  enable_testing()
  add_subdirectory("tests")
endif(RLZ_TESTS)

# Export variables
set(RLZ_INCLUDE_DIRS ${RLZ_INCLUDE_DIRS} CACHE INTERNAL "RLZ include directories")
list(APPEND RLZ_LIBRARIES "rlz_lib")
list(APPEND RLZ_LIBRARIES "sais")
list(APPEND RLZ_LIBRARIES "${SDSL_LIBRARIES}")
set(RLZ_LIBRARIES ${RLZ_LIBRARIES} CACHE INTERNAL "RLZ libraries")
set(RLZ_LIBRARY_DIRS "${RLZ_LIBRARY_DIRS}" CACHE INTERNAL "RLZ library path")