#include "main.hpp"
#include "serialize.hpp"

#include <algorithm>
#include <vector>

#include <bit_vectors.hpp>
#include <integer_type.hpp>
#include <prefix_sum.hpp>

template <typename Impl>
class Cumulative : public ::testing::Test {
public:
  std::vector<size_t> v;
  std::vector<size_t> prefix;
  const size_t extend       = 1024;
  const double probability  = 0.1;
  const size_t max_extend   = Impl::max_length;

  virtual void SetUp()
  {
    std::default_random_engine rg;
    std::uniform_real_distribution<> rd;
    size_t last = 0U;
    prefix.push_back(0U);
    for (auto i = 1U; i < extend; ++i) {
      if ((rd(rg) < probability) or (i - last >= max_extend)) {
        prefix.push_back(i);
        last = i;
      }
    }
    v = prefix;
    for (auto i = v.size() - 1; i > 0; --i) {
      v[i] -= v[i - 1];
    }
  }

  Impl get()
  {
    return Impl(v.begin(), v.end());
  }
};

template <typename It>
void print_range(It begin, It end) {
  for (auto it = begin; it != end; ++it) {
    std::cout << *it << " ";
  }
  std::cout << std::endl;
}

using CumulTypes = ::testing::Types<
  rlz::prefix::fast_cumulative<rlz::vectors::sparse>,
  rlz::prefix::fast_cumulative<rlz::vectors::dense>,
  rlz::prefix::cumulative<rlz::vectors::sparse>,
  rlz::prefix::cumulative<rlz::vectors::dense>,
  rlz::prefix::sampling_cumulative<>,
  rlz::prefix::sampling_cumulative<rlz::values::Size<4U>, rlz::values::Size<16U>, rlz::values::Size<16U>>
>;
TYPED_TEST_CASE(Cumulative, CumulTypes);

TYPED_TEST(Cumulative, Access)
{
  auto cum = this->get();
  for (auto i = 0U; i < this->v.size(); ++i) {
    ASSERT_EQ(this->v[i], cum.access(i));
  }
}

TYPED_TEST(Cumulative, Prefix)
{

  auto cum = this->get();
  for (auto i = 0U; i < this->prefix.size(); ++i) {
    ASSERT_EQ(this->prefix[i], cum.prefix(i));
  }
}

TYPED_TEST(Cumulative, SerialAccess)
{
  auto cum = this->get();
  cum = load_unload(cum);
  for (auto i = 0U; i < this->v.size(); ++i) {
    ASSERT_EQ(this->v[i], cum.access(i));
  }
}

TYPED_TEST(Cumulative, SerialPrefix)
{
  auto cum = this->get();
  cum = load_unload(cum);
  for (auto i = 0U; i < this->prefix.size(); ++i) {
    ASSERT_EQ(this->prefix[i], cum.prefix(i));
  }
}

TYPED_TEST(Cumulative, IteratorFull)
{
  auto cum     = this->get();
  auto cum_beg = cum.get_iterator(0);
  auto cum_end = cum.get_iterator_end();

  auto exp_beg = this->v.begin();
  auto exp_end = this->v.end();

  ASSERT_EQ(std::distance(cum_beg, cum_end), std::distance(exp_beg, exp_end));

  std::vector<size_t> exp_values(exp_beg, exp_end);
  std::vector<size_t> cum_values(cum_beg, cum_end);
  ASSERT_EQ(exp_values, cum_values);
}

TYPED_TEST(Cumulative, IteratorSub)
{
  auto cum = this->get();
  auto len = this->v.size();
  for (auto i = 0U; i < len; ++i) {
    for (auto j = i; j < len; ++j) {
      auto cum_beg = cum.get_iterator(i);
      auto cum_end = cum.get_iterator(j);
      auto exp_beg = std::next(this->v.begin(), i);
      auto exp_end = std::next(this->v.begin(), j);
      std::vector<size_t> exp_values(exp_beg, exp_end);
      std::vector<size_t> cum_values(cum_beg, cum_end);
      ASSERT_EQ(exp_values, cum_values);
    }
  }
}

TYPED_TEST(Cumulative, SerialIteratorFull)
{
  auto cum     = this->get();
  cum = load_unload(cum);
  auto cum_beg = cum.get_iterator(0);
  auto cum_end = cum.get_iterator_end();
  auto exp_beg = this->v.begin();
  auto exp_end = this->v.end();
  std::vector<size_t> exp_values(exp_beg, exp_end);
  std::vector<size_t> cum_values(cum_beg, cum_end);
  ASSERT_EQ(exp_values, cum_values);
}

TYPED_TEST(Cumulative, SerialIteratorSub)
{
  auto cum = this->get();
  cum = load_unload(cum);
  auto len = this->v.size();
  for (auto i = 0U; i < len; ++i) {
    for (auto j = i; j < len; ++j) {
      auto cum_beg = cum.get_iterator(i);
      auto cum_end = cum.get_iterator(j);
      auto exp_beg = std::next(this->v.begin(), i);
      auto exp_end = std::next(this->v.begin(), j);
      std::vector<size_t> exp_values(exp_beg, exp_end);
      std::vector<size_t> cum_values(cum_beg, cum_end);
      ASSERT_EQ(exp_values, cum_values);
    }
  }
}
