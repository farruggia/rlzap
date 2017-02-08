#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

#include <int_vector.hpp>

#include <boost/range.hpp>

#include <gtest/gtest.h>
#include "main.hpp"
#include "serialize.hpp"

class IntVector : public ::testing::Test {
public:
  std::array<int, 16> values;

  virtual void SetUp()
  {
    std::iota(values.begin(), values.end(), -8);
  }
};

TEST_F(IntVector, push_back_iterator)
{
  rlz::ds::int_vector<4> vec;
  for (auto i : this->values) {
    vec.push_back(i);
  }

  ASSERT_EQ(vec.size(), this->values.size());
  ASSERT_GE(vec.capacity(), vec.size());

  auto idx = 0U;
  for (auto i : vec) {
    int v = i;
    ASSERT_EQ(v, this->values[idx++]);
  }
}

TEST_F(IntVector, push_back_access)
{
  rlz::ds::int_vector<4> vec;
  for (auto i : this->values) {
    vec.push_back(i);
  }

  ASSERT_EQ(vec.size(), this->values.size());
  ASSERT_GE(vec.capacity(), vec.size());

  for (auto idx = 0U; idx < this->values.size(); ++idx) {
    int v = vec[idx];
    ASSERT_EQ(v, this->values[idx]);
  }
}

TEST_F(IntVector, access_iterator)
{
  rlz::ds::int_vector<4> vec(16, 0);

  auto idx = 0U;
  for (auto i : this->values) {
    vec[idx++] = i;
  }

  ASSERT_EQ(vec.size(), this->values.size());
  ASSERT_GE(vec.capacity(), vec.size());

  idx = 0U;
  for (auto i : vec) {
    int v = i;
    ASSERT_EQ(v, this->values[idx++]);
  }
}

TEST_F(IntVector, access_access)
{
  rlz::ds::int_vector<4> vec(16, 0);

  auto idx = 0U;
  for (auto i : this->values) {
    vec[idx++] = i;
  }

  ASSERT_EQ(vec.size(), this->values.size());
  ASSERT_GE(vec.capacity(), vec.size());

  for (auto idx = 0U; idx < this->values.size(); ++idx) {
    int v = vec[idx];
    ASSERT_EQ(v, this->values[idx]);
  }
}

TEST_F(IntVector, shrink_to_fit)
{
  rlz::ds::int_vector<4> vec;
  for (auto i : this->values) {
    vec.push_back(i);
    ASSERT_GE(vec.capacity(), vec.size());
    if (vec.capacity() > vec.size()) {
      vec.shrink_to_fit();
      ASSERT_EQ(vec.capacity(), vec.size());
      ASSERT_TRUE(std::equal(vec.begin(), vec.end(), this->values.begin()));
    }
  }
}

TEST_F(IntVector, serialize)
{
  rlz::ds::int_vector<4> vec(16, 0);

  auto idx = 0U;
  for (auto i : this->values) {
    vec[idx++] = i;
  }

  vec = load_unload(vec);

  ASSERT_EQ(vec.size(), this->values.size());
  ASSERT_GE(vec.capacity(), vec.size());

  for (auto idx = 0U; idx < this->values.size(); ++idx) {
    int v = vec[idx];
    ASSERT_EQ(v, this->values[idx]);
  }
}