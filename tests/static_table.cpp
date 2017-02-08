#include <algorithm>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <random>
#include <tuple>
#include <vector>

#include <fractional_container.hpp>
#include <integer_type.hpp>
#include <impl/prefix_sum.hpp>
#include <static_table_generation.hpp>

#include "main.hpp"

template <typename Bits>
class StaticTable : public ::testing::Test {
public:
  static const size_t bits     = Bits::value();
  static const size_t integers = 999;

  using container_t = rlz::fractional_byte_container<bits>;
  using byte        = std::uint8_t;

  using TableType = typename 
    rlz::static_table::generate_array<
      256U, 
      rlz::prefix::impl::table_gen::Generators<bits>:: template gen
    >::result;

  size_t get_bits()
  {
    return bits;
  }

  std::tuple<const std::uint8_t *, size_t> get_table()
  {
    const std::uint8_t *T = TableType::data;
    return std::make_tuple(T, sizeof(TableType::data));
  }

  container_t get_container()
  {
    // Generate some random integers
    std::default_random_engine dre(42U);
    std::uniform_int_distribution<byte>  dist(0, (1 << bits) - 1);
    std::vector<byte> to_ret(integers, 0);
    std::generate(to_ret.begin(), to_ret.end(), [&] () { return dist(dre); });
    return container_t(to_ret.begin(), to_ret.end());
  }
};

using Bits = ::testing::Types<
  rlz::values::Size<1>,
  rlz::values::Size<2>,
  rlz::values::Size<4>,
  rlz::values::Size<8>
>;

TYPED_TEST_CASE(StaticTable, Bits);

TYPED_TEST(StaticTable, Length)
{
  const std::uint8_t *T;
  size_t        length;
  std::tie(T, length) = this->get_table();
  ASSERT_EQ(256UL, length);
}

TYPED_TEST(StaticTable, Compute)
{
  const std::uint8_t *T;
  size_t        length;
  std::tie(T, length) = this->get_table();
  auto cont = this->get_container();

  const size_t bits     = this->get_bits();
  const size_t per_byte = 8UL / bits;

  // For each byte...
  for (auto i = 0U; i < cont.size(); i += per_byte) {
    // Get that byte
    auto val   = *std::next(cont.data(), i / per_byte);
    // Get its sum using the table
    auto t_sum = T[val];
    // Sum values using the container
    auto c_sum = 0U;
    for (auto j = i; j < std::min<size_t>(i + per_byte, cont.size()); ++j) {
      c_sum += cont[j];
    }
    ASSERT_EQ(t_sum, c_sum);
  }
}