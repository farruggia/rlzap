#include <bit_vectors.hpp>
#include <sdsl/bit_vectors.hpp>

#include <sstream>
#include <tuple>
#include <type_traits>

#include <boost/range.hpp>

#include <gtest/gtest.h>
#include "main.hpp"
#include "serialize.hpp"

bool power_of_two(size_t t)
{
  do {
    if ((t & 1) == 1) {
      t -= 1;
      if (t > 0) {
        return false;
      }
    }
    t >>= 1;
  } while (t > 0);
  return true;
}

size_t two_to_the(size_t i)
{
  size_t to_ret = 1;
  while (i--) {
    to_ret <<= 1;
  }
  return to_ret;
}


template <typename BvType>
struct BuilderFactory {
  BvType operator()(const std::vector<size_t> &v, size_t max)
  {
    BvType bv;
    using builder = typename BvType::builder;
    builder build {max};
    for (auto i : v) {
      build.set(i);
    }    
    bv = BvType(build.get());  
    return bv;
  }
};

template <typename BvType>
struct TupleFactory {
  BvType operator()(const std::vector<size_t> &v, size_t max)
  {
    return BvType{v.begin(), v.end(), max};
  }
};

template <typename BvType>
struct BitvectorFactory {
  BvType operator()(const std::vector<size_t> &v, size_t max)
  {
    auto bv = TupleFactory<rlz::vectors::BitVector<rlz::vectors::dense>>{}(v, max);
    return BvType{bv.data()};
  }
};

template <typename BvType>
struct SerializeFactory {
  BvType operator()(const std::vector<size_t> &v, size_t max)
  {
    auto bv = TupleFactory<BvType>{}(v, max);
    return load_unload(bv);
  }
};

template <typename Bv_, template <typename> class Builder_>
struct config {
  using Bv      = Bv_;
  using Builder = Builder_<Bv>;
};

template <typename T>
class BitVector : public ::testing::Test {
public:
  std::vector<size_t> v;
  const size_t max = 1ULL << 20;
  size_t max_rank;

  using Bv      = typename T::Bv;
  using Builder = typename T::Builder;

  virtual void SetUp()
  {
    max_rank = 0;
    for (auto i = 1U; i < max; i <<= 1) {
      v.push_back(i);
      ++max_rank;
    }
  }  

  Bv get()
  {
    return Builder{}(v, max);
  }

};

using rlz::vectors::sparse;
using rlz::vectors::dense;
using rlz::vectors::algorithms::rank;
using rlz::vectors::algorithms::bitset;

using VectorTypes = ::testing::Types<
  config<rlz::vectors::BitVector<sparse, rank, rlz::vectors::algorithms::select, bitset>,  BuilderFactory>,
  config<rlz::vectors::BitVector<dense,  rank, rlz::vectors::algorithms::select, bitset>,  BuilderFactory>,
  config<rlz::vectors::BitVector<sparse, rank, rlz::vectors::algorithms::select, bitset>,  TupleFactory>,
  config<rlz::vectors::BitVector<dense,  rank, rlz::vectors::algorithms::select, bitset>,  TupleFactory>,
  config<rlz::vectors::BitVector<sparse, rank, rlz::vectors::algorithms::select, bitset>,  BitvectorFactory>,
  config<rlz::vectors::BitVector<dense,  rank, rlz::vectors::algorithms::select, bitset>,  BitvectorFactory>,
  config<rlz::vectors::BitVector<sparse, rank, rlz::vectors::algorithms::select, bitset>,  SerializeFactory>,
  config<rlz::vectors::BitVector<dense,  rank, rlz::vectors::algorithms::select, bitset>,  SerializeFactory>
>;
TYPED_TEST_CASE(BitVector, VectorTypes);

template <typename Bv>
void check_ok(Bv &bv, std::vector<size_t> &v, size_t len)
{
  ASSERT_GT(bv.size(), 0);
  ASSERT_EQ(len, bv.size());
  for (auto i = 0U; i < bv.size(); ++i) {
    if (bv[i]) {
      ASSERT_FALSE(v.empty());
      ASSERT_EQ(i, v.front());
      v.erase(v.begin());
    }
  }  
}

TYPED_TEST(BitVector, Build)
{
  auto bv = this->get();
  ASSERT_NO_FATAL_FAILURE(check_ok(bv, this->v, this->max));
}

TYPED_TEST(BitVector, Rank)
{
  auto bv = this->get();
  // Test rank support
  size_t powers = 0U;
  for (auto i = 1U; i < bv.size(); ++i) {
    auto is_power = power_of_two(i);
    ASSERT_EQ(bv[i], is_power ? 1 : 0);
    ASSERT_EQ(bv.rank_1(i), powers);
    powers += is_power;
  }  
}

TYPED_TEST(BitVector, Select)
{
  auto bv = this->get();
  for (auto i = 1U; i <= this->max_rank; ++i) {
    ASSERT_EQ(bv.select_1(i), two_to_the(i - 1));
  }
}

TYPED_TEST(BitVector, BitsetFull)
{
  auto bv     = this->get();
  auto begin  = bv.get_bitset(0UL);
  auto end    = bv.get_bitset_end();

  // print_range(bv.begin(), bv.end());
  // print_range(this->v.begin(), this->v.end());
  // print_range(begin, end);
  ASSERT_EQ(this->v.size(), std::distance(begin, end));

  auto exp_beg = this->v.begin();
  auto exp_end = this->v.end();
  ASSERT_EQ(boost::make_iterator_range(begin, end), boost::make_iterator_range(exp_beg, exp_end));  
}

TYPED_TEST(BitVector, BitsetPartial)
{
  auto bv     = this->get();
  auto len    = this->v.size();
  for (auto i = 0U; i < len; ++i) {
    for (auto j = i; j < len; ++j) {
      auto begin    = bv.get_bitset(i);
      auto end      = bv.get_bitset(j);
      auto exp_beg  = std::next(this->v.begin(), i);
      auto exp_end  = std::next(this->v.begin(), j);

      ASSERT_EQ(j - i, std::distance(begin, end));
      ASSERT_EQ(boost::make_iterator_range(begin, end), boost::make_iterator_range(exp_beg, exp_end));
    }
  }
}