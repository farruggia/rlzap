# add_subdirectory("ext_libs/gtest/googletest")

include_directories("${gtest_SOURCE_DIR}/include")

function(test_add test_name binary)
  add_executable(${binary} ${binary}.cpp)
  add_dependencies(${binary} gtest)
  target_link_libraries(${binary} ${RLZ_LIBRARIES} ${Boost_LIBRARIES} gtest)  
  add_test(${test_name} "${CMAKE_CURRENT_BINARY_DIR}/${binary}" "--gtest_color=yes")
  if (PRECOMPILE)
    cotire(${binary})
  endif(PRECOMPILE)
endfunction()

test_add(SaGetter sa_getter)
test_add(GetMatchings get_matchings)
# test_add(ClassicParse classic_parse)
test_add(RlzapParse parse_rlzap)
test_add(LcpParse parse_lcp)
test_add(MatchSerialize serialize_matches)
test_add(NewVector int_vector_specialization)
test_add(Fractional fractional_byte_vector)
test_add(BitVectors bit_vectors)
test_add(IntVector int_vector)
test_add(DiffIterator diff_iterator)
test_add(SDVectors sparse_dense_vectors)
test_add(StaticTable static_table)
test_add(Cumulative cumulative)
test_add(Coordinator coordinator)
test_add(LcpCoordinator, lcp_coordinator)
# test_add(DnaPack dna_pack)
test_add(LcpPack lcp_pack)
# test_add(ClassicParseKeeper classic_parse_keeper)
test_add(ParseKeeper parse_keeper)
test_add(LiteralKeeper literal_keeper)
# test_add(ClassicLiteralKeeper classic_literal_keeper)
test_add(Index index)
test_add(LcpIndex lcp_index)
test_add(Api api)
test_add(LcpApi lcp_api)
test_add(NoRandomAccess no_random_access)

# SLA tester
add_library(sla libs/sla.cpp)

add_executable(sla_create sla_create.cpp)
target_link_libraries(sla_create sla ${RLZ_LIBRARIES} ${Boost_LIBRARIES})

add_executable(sla_print sla_print.cpp)
target_link_libraries(sla_print sla ${RLZ_LIBRARIES} ${Boost_LIBRARIES})

add_executable(sla_build_index sla_build_index.cpp)
target_link_libraries(sla_build_index sla ${RLZ_LIBRARIES} ${Boost_LIBRARIES})

add_executable(sla_test_index sla_test_index.cpp)
target_link_libraries(sla_test_index sla ${RLZ_LIBRARIES} ${Boost_LIBRARIES})