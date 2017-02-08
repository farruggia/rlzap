include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(sais source/sais.c)

set(SAIS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)