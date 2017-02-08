#include <iterator>
#include <vector>

#include <integer_type.hpp>

#include <sdsl_extensions/int_vector.hpp>

#include "main.hpp"

template <typename Value>
class IntVector : public ::testing::Test {
  std::vector<unsigned int> check_;
public:

  using vec_type = sdsl::extensions::int_vector<Value::value()>;

  virtual void SetUp() {
    auto top = (1UL << Value::value());
    for (auto i = 0U; i < 3 * top; ++i) {
      check_.push_back(i % top);
    }
  }

  std::vector<unsigned int> check()
  {
    return check_;
  }

  vec_type get()
  {
    vec_type to_ret(check_.size(), 0U);
    for (auto i = 0U; i < check_.size(); ++i) {
      to_ret[i] = check_[i];
    }
    return to_ret;
  }
};

using values = ::testing::Types<rlz::values::Size<4UL>, rlz::values::Size<2UL>>;

TYPED_TEST_CASE(IntVector, values);

TYPED_TEST(IntVector, size)
{
  auto exp = this->check();
  auto cmp = this->get();
  ASSERT_EQ(exp.size(), cmp.size());
}

TYPED_TEST(IntVector, FullIterator)
{
  auto exp    = this->check();
  auto cmp    = this->get();
  auto begin  = cmp.begin();
  auto end    = cmp.end();
  for (auto i : exp) {
    ASSERT_NE(begin, end);
    unsigned int val = *begin++;
    ASSERT_EQ(i, val);
  }
  ASSERT_EQ(begin, end);
}

TYPED_TEST(IntVector, WriteIterator)
{
  using VecType = typename IntVector<TypeParam>::vec_type;
  auto exp    = this->check();
  VecType cmp(exp.size(), 0U);

  auto begin  = cmp.begin();
  auto end    = cmp.end();

  for (auto i : exp) {
    ASSERT_NE(begin, end);
    *begin++ = i;
  }
  ASSERT_EQ(begin, end);

  begin = cmp.begin();
  end   = cmp.end();
  for (auto i : exp) {
    ASSERT_NE(begin, end);
    unsigned int val = *begin++;
    ASSERT_EQ(i, val);
  }
  ASSERT_EQ(begin, end);
}

TYPED_TEST(IntVector, RandomAccess)
{
  auto exp  = this->check();
  auto cmp  = this->get();

  for (auto i = 0U; i < exp.size(); ++i) {
    unsigned int v = cmp[i];
    ASSERT_EQ(exp[i], v);
  }
}

TYPED_TEST(IntVector, ConstRandomAccess)
{
  using VecType = typename IntVector<TypeParam>::vec_type;
  auto exp  = this->check();
  auto cmp  = this->get();
  const VecType c_cmp = cmp;

  for (auto i = 0U; i < exp.size(); ++i) {
    unsigned int v = c_cmp[i];
    ASSERT_EQ(exp[i], v);
  }
}

TYPED_TEST(IntVector, ConstIterator)
{
  using VecType = typename IntVector<TypeParam>::vec_type;
  auto exp  = this->check();
  auto cmp  = this->get();
  const VecType c_cmp = cmp;

  auto exp_begin = exp.begin();
  auto exp_end   = exp.end();
  auto cmp_begin = c_cmp.begin();
  auto cmp_end   = c_cmp.end();

  while (cmp_begin != cmp_end) {
    unsigned int v = *cmp_begin++;
    ASSERT_EQ(*exp_begin++, v);
  }
  ASSERT_EQ(cmp_begin, cmp_end);
  ASSERT_EQ(exp_begin, exp_end);
}

TYPED_TEST(IntVector, Distance)
{
  auto cmp  = this->get();
  for (auto i = 0U; i < cmp.size(); i++) {
    auto begin = cmp.begin();
    auto next  = std::next(begin, i);
    ASSERT_EQ(i, std::distance(begin, next));
  }  
}
