#include <algorithm>
#include <diff_iterator.hpp>
#include "main.hpp"

#include <boost/range.hpp>

class DiffIterator : public ::testing::Test {
public:
  std::vector<int> v;
  std::vector<int> diff;

  using Iter = rlz::utils::iterator_diff<std::vector<int>::iterator, int>;

  virtual void SetUp()
  {
    std::vector<int> copy       {{ 4, 8, 9, 15 }};
    std::vector<int> diff_copy  {{ 4, 4, 1, 6}};
    v     = copy;
    diff  = diff_copy;
  }
};

template <typename It>
void print_range(It begin, It end)
{
  for (auto it = begin; it != end; ++it) {
    std::cout << *it << " ";
  }
  std::cout << std::endl;
}

TEST_F(DiffIterator, Whole)
{
  Iter beg { v.begin(), v.begin() };
  Iter end = Iter::end_iterator(v.end());
  auto diff_begin = diff.begin(), diff_end = diff.end();
  ASSERT_EQ(boost::make_iterator_range(beg, end), boost::make_iterator_range(diff_begin, diff_end));
}

TEST_F(DiffIterator, Slice)
{
  for (auto i = 0U; i < v.size(); ++i) {
    for (auto j = i; j < v.size(); ++j) {
      Iter beg { v.begin(), std::next(v.begin(), i) };
      Iter end { v.begin(), std::next(v.begin(), j)};
      auto diff_begin = std::next(diff.begin(), i);
      auto diff_end   = std::next(diff.begin(), j);
      ASSERT_EQ(boost::make_iterator_range(beg, end), boost::make_iterator_range(diff_begin, diff_end));
    }
  }
}

TEST_F(DiffIterator, Prefix)
{
  for (auto i = 0U; i < v.size(); ++i) {
    Iter beg { v.begin() };
    Iter end { v.begin(), std::next(v.begin(), i) };
    auto diff_begin = diff.begin();
    auto diff_end   = std::next(diff_begin, i);
    ASSERT_EQ(boost::make_iterator_range(beg, end), boost::make_iterator_range(diff_begin, diff_end));
  }
}