#include <algorithm>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

#include <fractional_container.hpp>
#include <integer_type.hpp>
#include <type_utils.hpp>

#include "main.hpp"

using byte = std::uint8_t;

static const size_t integers = 1000;

template <typename List>
struct Unpack {

};

template <typename Size, typename Serialize>
struct Unpack<rlz::type_utils::type_list<Size, Serialize>> {
  static constexpr size_t bits() { return Size::value(); }
  static constexpr bool serialize() { return Serialize::value(); }
};

template <typename Config>
class Fractional : public ::testing::Test {
public:
  const static size_t bits      = Unpack<Config>::bits();
  const static bool   serialize = Unpack<Config>::serialize();
  using container_t = rlz::fractional_byte_container<bits>;

  std::vector<byte> get_test_data()
  {
    // Generate some random integers
    std::default_random_engine dre(42U);
    std::uniform_int_distribution<byte>  dist(0, (1 << bits) - 1);
    std::vector<byte> to_ret(integers, 0);
    std::generate(to_ret.begin(), to_ret.end(), [&] () { return dist(dre); });
    return to_ret;
  }

  container_t get_container()
  {
    auto data = get_test_data();
    auto cont = container_t(data.begin(), data.end());
    if (serialize) {
      std::stringstream ss;
      cont.serialize(ss);
      auto rep = ss.str();
      cont = container_t{};
      std::stringstream new_ss(rep);
      cont.load(new_ss);
    }
    return cont;
  }
};

using Configs = ::testing::Types<
  rlz::type_utils::type_list<rlz::values::Size<1>, rlz::values::Boolean<false>>,
  rlz::type_utils::type_list<rlz::values::Size<2>, rlz::values::Boolean<false>>,
  rlz::type_utils::type_list<rlz::values::Size<4>, rlz::values::Boolean<false>>,
  rlz::type_utils::type_list<rlz::values::Size<8>, rlz::values::Boolean<false>>,
  rlz::type_utils::type_list<rlz::values::Size<1>, rlz::values::Boolean<true>>,
  rlz::type_utils::type_list<rlz::values::Size<2>, rlz::values::Boolean<true>>,
  rlz::type_utils::type_list<rlz::values::Size<4>, rlz::values::Boolean<true>>,
  rlz::type_utils::type_list<rlz::values::Size<8>, rlz::values::Boolean<true>>
>;

TYPED_TEST_CASE(Fractional, Configs);

TYPED_TEST(Fractional, Size)
{
  auto values = this->get_test_data();
  auto cont   = this->get_container();
  ASSERT_EQ(values.size(), cont.size());
}

TYPED_TEST(Fractional, Access)
{
  auto values = this->get_test_data();
  auto cont   = this->get_container();
  auto size   = values.size();
  for (auto i = 0U; i < size; ++i) {
    ASSERT_EQ(values[i], cont[i]);
  }
}

TYPED_TEST(Fractional, SeqAccess)
{
  auto values = this->get_test_data();
  auto cont   = this->get_container();
  auto v_it   = values.begin();
  auto c_it   = cont.begin();
  for (; v_it != values.end(); ++v_it, ++c_it) {
    ASSERT_NE(c_it, cont.end());
    auto v_val = *v_it;
    auto c_val = *c_it;
    ASSERT_EQ(v_val, c_val);
  }
  ASSERT_EQ(c_it, cont.end());
}

TYPED_TEST(Fractional, RandomAccess)
{
  auto values = this->get_test_data();
  auto cont   = this->get_container();
  auto v_it   = values.begin();
  auto c_it   = cont.begin();

  std::vector<int> positions(values.size() - 1, 0);
  std::iota(positions.begin(), positions.end(), 1);
  std::shuffle(positions.begin(), positions.end(), std::default_random_engine(42));

  auto prev_pos = 0;
  for (auto i : positions) {
    auto jump = i - prev_pos;
    std::advance(v_it, jump);
    std::advance(c_it, jump);
    ASSERT_NE(v_it, values.end());
    ASSERT_NE(c_it, cont.end());
    auto v_val = *v_it;
    auto c_val = *c_it;
    ASSERT_EQ(v_val, c_val);
    prev_pos = i;
  }
}

TYPED_TEST(Fractional, IteratorDistance)
{
  auto cont   = this->get_container();
  auto c_beg  = cont.begin();
  auto c_end  = cont.end();
  ASSERT_EQ(std::distance(c_beg, c_end), cont.size());
  for (auto i = 0; static_cast<size_t>(i) < cont.size(); ++i) {
    for (auto j = 0; static_cast<size_t>(j) < cont.size(); ++j) {
      auto c_start = std::next(c_beg, i);
      auto c_end   = std::next(c_beg, j);
      ASSERT_EQ(std::distance(c_start, c_end), j - i);
    }
  }
}
