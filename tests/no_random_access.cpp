#include <lcp/api.hpp>

#include <boost/iterator/iterator_adaptor.hpp>

#include <csignal>
#include <iterator>
#include <random>

#include "include/parse_coord_common.hpp"
#include "main.hpp"

template <
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
  std::size_t _ExplicitBits,
  std::size_t _DeltaBits,
  std::size_t _MaxLit,
  std::size_t _SampleInt,
  std::size_t _MaxLen,
  std::size_t _DocLen
>
struct unpack<type_pack<_ExplicitBits, _DeltaBits, _MaxLit, _SampleInt, _MaxLen, _DocLen>>
{
  constexpr static std::size_t ExplicitBits() { return _ExplicitBits; }
  constexpr static std::size_t DeltaBits()    { return _DeltaBits; }
  constexpr static std::size_t MaxLit()       { return _MaxLit; }
  constexpr static std::size_t SampleInt()    { return _SampleInt; }
  constexpr static std::size_t MaxLen()       { return _MaxLen; }
  constexpr static std::size_t DocLen()       { return _DocLen; }
};

template <typename ConfigTuple>
class NoRandomAccess : public ::testing::Test {
};

using IndexTypes = ::testing::Types<
  type_pack<32UL, 4UL, 2UL, 64UL, 1024UL, 262144UL>,
  type_pack<32UL, 2UL, 4UL, 48UL,  512UL, 262144UL>,
  type_pack<16UL, 4UL, 2UL, 64UL,  256UL,    16384>,
  type_pack<16UL, 2UL, 4UL, 48UL,  128UL,    16384>,
  type_pack<48UL, 4UL, 2UL, 64UL,   64UL, 262144UL>,
  type_pack<48UL, 2UL, 4UL, 48UL,   32UL, 262144UL>,
  type_pack<32UL, 4UL, 2UL, 64UL, 1024UL, 262144UL>,
  type_pack<32UL, 2UL, 4UL, 48UL,  512UL, 262144UL>,
  type_pack<16UL, 4UL, 2UL, 64UL,  256UL,    16384>,
  type_pack<16UL, 2UL, 4UL, 48UL,  128UL,    16384>,
  type_pack<48UL, 4UL, 2UL, 64UL,   64UL, 262144UL>,
  type_pack<48UL, 2UL, 4UL, 48UL,   32UL, 262144UL>
>;

TYPED_TEST_CASE(NoRandomAccess, IndexTypes);

class jump_count
  : public boost::iterator_adaptor
  <
    jump_count,    // Derived
    std::uint32_t* // Base
  >
{
 private:
    std::size_t * jumps_;
 public:
    jump_count()
      : jump_count::iterator_adaptor_(nullptr),
        jumps_(nullptr)
    {}

    explicit jump_count(std::uint32_t *ptr, std::size_t *jump)
      : jump_count::iterator_adaptor_(ptr),
        jumps_(jump)
    {}

    jump_count(jump_count const& other) 
      : jump_count(other.base(), other.jumps_)
    {}

    jump_count& operator=(const jump_count& other)
    {
      base_reference() = other.base_reference();
      jumps_           = other.jumps_;
    }

    std::size_t jumps() const
    {
      return *jumps_;
    }

 private:
    friend class boost::iterator_core_access;

    void advance(typename iterator_adaptor::difference_type n)
    {
      if (n > 1 or n < -1) {
        #if 0
        if (should_raise_signal) {
          std::raise(SIGINT);
        }
        #endif
        ++*jumps_;
      }
      base_reference() = std::next(base_reference(), n);
    }
};

constexpr const double alpha = 1;

TYPED_TEST(NoRandomAccess, RandomPhraseAccess)
{
  using LcpType = std::uint32_t;
  constexpr std::size_t ExplicitBits = unpack<TypeParam>::ExplicitBits();
  constexpr std::size_t DeltaBits    = unpack<TypeParam>::DeltaBits();
  constexpr std::size_t MaxLit       = unpack<TypeParam>::MaxLit();
  constexpr std::size_t SampleInt    = unpack<TypeParam>::SampleInt();
  constexpr std::size_t MaxLen       = unpack<TypeParam>::MaxLen();
  constexpr std::size_t DocLen       = unpack<TypeParam>::DocLen();

  auto input     = ds_getter<DocLen>{}.input();
  auto reference = ds_getter<DocLen>{}.reference();

  // Input iterators
  auto input_begin     = input.data();
  auto input_end       = input.data() + input.size();
  // Reference source iterator
  auto reference_begin = reference.data();
  auto reference_end   = reference.data() + reference.size();
  // Reference wrapper
  std::size_t jumps = 0U;
  auto wrap_begin = jump_count(reference_begin, &jumps);
  auto wrap_end   = jump_count(reference_end, &jumps);

  auto idx =  rlz::lcp::index_build
  <
    LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt
  >
  (
    input_begin, input_end,
    wrap_begin, wrap_end,
    MaxLen
  );

  jumps = 0U;

  // Randomly sample the input range, getting a phrase and reading it all.
  // Number of jumps must be not more than the 
  std::default_random_engine rg(42U);
  std::uniform_int_distribution<std::size_t> dist(0U, input.size() - 1);
  for (auto i = 0U; i < alpha * input.size(); ++i) {
    using iter = typename decltype(idx)::iter;
    iter beg, middle, end;
    std::tie(beg, middle, end) = idx.iterator_position(dist(rg));
    std::vector<std::uint32_t> output;
    std::copy(beg, end, std::back_inserter(output));
  }
  ASSERT_LT(jumps, 1.5 * input.size());
}
