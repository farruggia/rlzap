#include <lcp/api.hpp>

#include <numeric>

#include "include/parse_coord_common.hpp"
#include "main.hpp"

template <
  typename _ReferenceIt,
  typename _LcpType,
  std::size_t _ExplicitBits,
  std::size_t _DeltaBits,
  std::size_t _MaxLit,
  std::size_t _SampleInt,
  std::size_t _MaxLen,
  std::size_t _DocLen
>
struct type_pack { };

template <typename TypePack>
struct unpack
{ };

template <
  typename _ReferenceIt,
  typename _LcpType,
  std::size_t _ExplicitBits,
  std::size_t _DeltaBits,
  std::size_t _MaxLit,
  std::size_t _SampleInt,
  std::size_t _MaxLen,
  std::size_t _DocLen
>
struct unpack<type_pack<_ReferenceIt, _LcpType, _ExplicitBits, _DeltaBits, _MaxLit, _SampleInt, _MaxLen, _DocLen>>
{
  using ReferenceIt = _ReferenceIt;
  using LcpType = _LcpType;
  constexpr static std::size_t ExplicitBits() { return _ExplicitBits; }
  constexpr static std::size_t DeltaBits()    { return _DeltaBits; }
  constexpr static std::size_t MaxLit()       { return _MaxLit; }
  constexpr static std::size_t SampleInt()    { return _SampleInt; }
  constexpr static std::size_t MaxLen()       { return _MaxLen; }
  constexpr static std::size_t DocLen()       { return _DocLen; }
};

template <typename ConfigTuple>
class LcpApi : public ::testing::Test {
};

using IndexTypes = ::testing::Types<
  type_pack<std::uint32_t*, std::uint32_t, 32UL, 4UL, 2UL, 64UL, 1024UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 32UL, 2UL, 4UL, 48UL,  512UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 16UL, 4UL, 2UL, 64UL,  256UL,    16384>,
  type_pack<std::uint32_t*, std::uint32_t, 16UL, 2UL, 4UL, 48UL,  128UL,    16384>,
  type_pack<std::uint32_t*, std::uint32_t, 48UL, 4UL, 2UL, 64UL,   64UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 48UL, 2UL, 4UL, 48UL,   32UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 32UL, 4UL, 2UL, 64UL, 1024UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 32UL, 2UL, 4UL, 48UL,  512UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 16UL, 4UL, 2UL, 64UL,  256UL,    16384>,
  type_pack<std::uint32_t*, std::uint32_t, 16UL, 2UL, 4UL, 48UL,  128UL,    16384>,
  type_pack<std::uint32_t*, std::uint32_t, 48UL, 4UL, 2UL, 64UL,   64UL, 262144UL>,
  type_pack<std::uint32_t*, std::uint32_t, 48UL, 2UL, 4UL, 48UL,   32UL, 262144UL>
>;

TYPED_TEST_CASE(LcpApi, IndexTypes);

template <typename Idx>
::testing::AssertionResult check_limit(Idx &idx, std::size_t phrase_limit)
{
  auto to_ret = ::testing::AssertionSuccess();
  auto check_func = [&] (std::size_t phrase_start, std::size_t lit_len, std::size_t, std::size_t copy_len) -> void {
    auto phrase_len = lit_len + copy_len;
    if (phrase_len > phrase_limit) {
      to_ret = ::testing::AssertionFailure() << "Length of phrase starting at "<< phrase_start
                                             << " is of length " << phrase_len
                                             << " but phrase limit is " << phrase_limit;
    }
  };
  idx.process_parsing(check_func);
  return to_ret;
}

TYPED_TEST(LcpApi, Range)
{
  using ReferenceIt = typename unpack<TypeParam>::ReferenceIt;
  using LcpType     = typename unpack<TypeParam>::LcpType;
  constexpr std::size_t ExplicitBits = unpack<TypeParam>::ExplicitBits();
  constexpr std::size_t DeltaBits    = unpack<TypeParam>::DeltaBits();
  constexpr std::size_t MaxLit       = unpack<TypeParam>::MaxLit();
  constexpr std::size_t SampleInt    = unpack<TypeParam>::SampleInt();
  constexpr std::size_t MaxLen       = unpack<TypeParam>::MaxLen();
  constexpr std::size_t DocLen       = unpack<TypeParam>::DocLen();

  auto input     = ds_getter<DocLen>{}.input();
  auto reference = ds_getter<DocLen>{}.reference();

  auto input_begin = input.data();
  auto input_end   = input.data() + input.size();
  auto reference_begin = reference.data();
  auto reference_end   = reference.data() + reference.size();

  auto idx =  rlz::lcp::index_build<
                LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt
              > (
                input_begin, input_end,
                reference_begin, reference_end,
                MaxLen
              );

  // Check correctness
  std::vector<LcpType> read(input.size());
  ASSERT_EQ(idx.size(), input.size());
  idx(0UL, input.size(), read.begin());
  ASSERT_EQ(input, read);

  // Check phrase limit
  EXPECT_TRUE(check_limit(idx, MaxLen));
}

TYPED_TEST(LcpApi, PhraseLen)
{
  using ReferenceIt = typename unpack<TypeParam>::ReferenceIt;
  using LcpType     = typename unpack<TypeParam>::LcpType;
  constexpr std::size_t ExplicitBits = unpack<TypeParam>::ExplicitBits();
  constexpr std::size_t DeltaBits    = unpack<TypeParam>::DeltaBits();
  constexpr std::size_t MaxLit       = unpack<TypeParam>::MaxLit();
  constexpr std::size_t SampleInt    = unpack<TypeParam>::SampleInt();
  constexpr std::size_t MaxLen       = unpack<TypeParam>::MaxLen();
  constexpr std::size_t DocLen       = unpack<TypeParam>::DocLen();

  std::vector<LcpType> input(DocLen, LcpType{});
  auto reference = input;

  auto input_begin = input.data();
  auto input_end   = input.data() + input.size();
  auto reference_begin = reference.data();
  auto reference_end   = reference.data() + reference.size();

  auto idx =  rlz::lcp::index_build<
                LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt
              > (
                input_begin, input_end,
                reference_begin, reference_end,
                MaxLen
              );

  // Check phrase limit
  EXPECT_TRUE(check_limit(idx, MaxLen));
}

template <typename TypeParam>
::testing::AssertionResult input_runs(unsigned int input_start, unsigned int ref_start)
{
  using ReferenceIt = typename unpack<TypeParam>::ReferenceIt;
  using LcpType     = typename unpack<TypeParam>::LcpType;
  constexpr std::size_t ExplicitBits = unpack<TypeParam>::ExplicitBits();
  constexpr std::size_t DeltaBits    = unpack<TypeParam>::DeltaBits();
  constexpr std::size_t MaxLit       = unpack<TypeParam>::MaxLit();
  constexpr std::size_t SampleInt    = unpack<TypeParam>::SampleInt();
  constexpr std::size_t MaxLen       = unpack<TypeParam>::MaxLen();
  constexpr std::size_t DocLen       = unpack<TypeParam>::DocLen();

  std::vector<LcpType> input(DocLen), reference(DocLen);
  std::iota(input.begin(), input.end(), input_start);
  std::iota(reference.begin(), reference.end(), ref_start);

  auto input_begin     = input.data();
  auto input_end       = input.data() + input.size();
  auto reference_begin = reference.data();
  auto reference_end   = reference.data() + reference.size();

  auto idx =  rlz::lcp::index_build<
                LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt
              > (
                input_begin, input_end,
                reference_begin, reference_end,
                MaxLen
              );

  // auto F = [] (
  //     std::size_t phrase_start, std::size_t lit_len, std::size_t target, std::size_t copy_len
  //   ) {
  //     if (lit_len > 0) {
  //       std::cout << "L(" << lit_len << "): "
  //                 << phrase_start << " -> " << (phrase_start + lit_len)
  //                 << std::endl;
  //       phrase_start += lit_len;        
  //     }
  //     if (copy_len > 0) {
  //       std::cout << "C(" << copy_len << "): "
  //                 << phrase_start << " -> " << (phrase_start + copy_len)
  //                 << " @ " << target
  //                 << std::endl;
  //     }
  // };
  // idx.process_parsing(F);

  // Check phrase limit
  auto check_ass = check_limit(idx, MaxLen);
  if (check_ass != ::testing::AssertionSuccess()) {
    return check_ass;
  }

  // Check if decompress properly
  auto got = idx(0U, input.size());
  if (got != input) {
    return ::testing::AssertionFailure() << "Input and reference differ";
  }
  return ::testing::AssertionSuccess();
}


TYPED_TEST(LcpApi, SplitNoCompulsoryLiteral)
{
  EXPECT_TRUE(input_runs<TypeParam>(0U, 0U));
}

TYPED_TEST(LcpApi, SplitCompulsoryLiteral)
{
  EXPECT_TRUE(input_runs<TypeParam>(0U, 1U));
}