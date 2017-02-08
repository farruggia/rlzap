#include <lcp/api.hpp>

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

  std::vector<LcpType> read(input.size());
  ASSERT_EQ(idx.size(), input.size());
  idx(0UL, input.size(), read.begin());
  ASSERT_EQ(input, read);
}
