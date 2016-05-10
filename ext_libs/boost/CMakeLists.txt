file(GLOB boost_files ./libs/*.cpp)
set(INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(BOOST_INCLUDE_DIR "${INCLUDE}" PARENT_SCOPE)

include_directories(${INCLUDE})
add_library(boost STATIC ${boost_files})
