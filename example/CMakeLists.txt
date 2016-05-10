# Assuming to place the root of this project on ext_libs/rlzap
add_subdirectory(ext_libs/rlzap)

include_directories(${RLZ_INCLUDE_DIRS})

set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -O3 -march=native -ffast-math -funroll-loops -DNDEBUG")

add_executable(rlz_examples rlz_examples.cpp)
target_link_libraries(rlz_examples sdsl ${RLZ_LIBRARIES})
