file(GLOB boost_files ./libs/*.cpp)
set(INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(Boost_INCLUDE_DIR "${INCLUDE}" CACHE INTERNAL "Boost include directories")

include_directories(${INCLUDE})
add_library(boost STATIC ${boost_files})
set(Boost_LIBRARIES "boost" CACHE INTERNAL "Boost libraries")