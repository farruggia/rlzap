#include <integer_type.hpp>
#include <sparse_dense_vector.hpp>

#include <vector>
#include <sstream>

#include "main.hpp"

std::vector<bool> bits {{
      true,  true,  true,  true,  true,  false, false, false,  // 1
      true,  false, true,  false, true,  false, true,  false,  // 1
	true,  false, false, false, false, false, false, false,  // 1
	false, false, false, false, false, false, false, false,  // 0
	false, false, false, false, false, false, false, false,  // 0
	false, false, false, true,  true,  true,  true,  true,   // 1
	true,  true,  true,  true,  true,  true,  true,  true,   // 1
	true,  true,  true,  true,  true,  true,  true,  true,   // 1
	true,  true,  true,  true,  true,  true,  true,  false,  // 1
	true,  false, true,  false, false, true,  false, false,  // 1
	false, false, false, false, false, false, false, false,  // 0
	false, false, false, false, false, false, false, false,  // 0
	false, false, false, false, false, false, false, false,  // 0
	false, false, false, false, false, false, false, false,  // 0
	true,  false, true                                       // 1
}};

template <typename ChunkSize>
using VecType = rlz::vectors::sd_bitvector<ChunkSize>;

struct filled_bits {
  std::vector<bool> operator()()
  {
    return bits;
  }
};

struct empty_bits {
  std::vector<bool> operator()()
  {
    auto to_ret = std::vector<bool>(bits.size(), false);
    return to_ret;
  }
};

template <typename ChunkSize>
struct instantiate {
  VecType<ChunkSize> operator()(std::vector<bool> bits)
  {
    auto size = bits.size();
    std::vector<size_t> positions;
    auto idx = 0UL;
    for (auto i : bits) {
      if (i == 1) { positions.push_back(idx); }
      ++idx;
    }
    return VecType<ChunkSize> { positions.begin(), positions.end(), size };    
  }  
};

template <typename ChunkSize>
struct serialize {
  VecType<ChunkSize> operator()(std::vector<bool> bits)
  {
    instantiate<ChunkSize> inst;
    auto to_serialize = inst(bits);
    std::stringstream output;
    to_serialize.serialize(output);
    std::stringstream input(output.str());
    VecType<ChunkSize> v;
    v.load(input);
    return v;
  }
};

template <typename, typename, template <typename> class>
struct type_list { };

template <typename T>
class SdVector {

};

template <typename BitGetter, typename ChunkSize, template <typename> class Getter>
class SdVector<type_list<BitGetter, ChunkSize, Getter>> : public ::testing::Test {
public:

  std::vector<bool> get_bits()
  {
    return BitGetter{}();
  }


  VecType<ChunkSize> get()
  {
    return Getter<ChunkSize>{}(get_bits());
  }
};

using Types = ::testing::Types<
  type_list<filled_bits, rlz::values::Size<8>, instantiate>,
  type_list<filled_bits, rlz::values::Size<16>, instantiate>,
  type_list<filled_bits, rlz::values::Size<32>, instantiate>,
  type_list<filled_bits, rlz::values::Size<8>, serialize>,
  type_list<filled_bits, rlz::values::Size<16>, serialize>,
  type_list<filled_bits, rlz::values::Size<32>, serialize>,
  type_list<empty_bits, rlz::values::Size<8>, instantiate>,
  type_list<empty_bits, rlz::values::Size<16>, instantiate>,
  type_list<empty_bits, rlz::values::Size<32>, instantiate>,
  type_list<empty_bits, rlz::values::Size<8>, serialize>,
  type_list<empty_bits, rlz::values::Size<16>, serialize>,
  type_list<empty_bits, rlz::values::Size<32>, serialize>
>;
TYPED_TEST_CASE(SdVector, Types);

TYPED_TEST(SdVector, length)
{
  auto bits = this->get_bits();
  auto cmp  = this->get();
  ASSERT_EQ(bits.size(), cmp.length());
}

TYPED_TEST(SdVector, access)
{
  auto bits = this->get_bits();
  auto cmp  = this->get();
  for (auto i = 0U; i < bits.size(); ++i) {
    // std::cout << i << "\t" << bits[i] << "\t" << cmp[i] << std::endl;
    ASSERT_EQ(bits[i], cmp[i]);
  }
}

TYPED_TEST(SdVector, FullIteration)
{
  auto bits  = this->get_bits();
  auto cmp   = this->get();
  auto e_it  = bits.begin();
  auto c_it  = cmp.begin();
  auto e_end = bits.end();
  auto c_end = cmp.end();
  ASSERT_EQ(std::distance(e_it, e_end), std::distance(c_it, c_end));
  while (e_it != e_end) {
    ASSERT_NE(c_it, c_end);
    ASSERT_EQ(*e_it++, *c_it++);
  }
  ASSERT_EQ(c_end, c_it);
}

TYPED_TEST(SdVector, Distance)
{
  auto cmp = this->get();
  auto len = cmp.length();
  for (auto i = 0U; i < len; ++i) {
    auto start = cmp.from(i);
    for (auto j = i; j < len; ++j) {
      auto end = cmp.from(j);
      ASSERT_EQ(j - i, std::distance(start, end));
    }
    ASSERT_EQ(len - i, std::distance(start, cmp.end()));
  }
}

TYPED_TEST(SdVector, Skipping)
{
  auto cmp = this->get();
  auto exp = this->get_bits();
  std::vector<int> idx {{0, 1, 2, 4, 8, 16, 32, 16, 8, 4, 2, 1, 0}};
  auto start = cmp.begin();

  auto last_idx = 0;
  for (auto i : idx) {
    std::advance(start, i - last_idx);
    ASSERT_EQ(exp[i], *start);
    last_idx = i;
  }
}