#include <sstream>
#include <vector>

#include <match_serialize.hpp>
#include "main.hpp"

namespace rlz {

bool operator==(const match &m_1, const match &m_2)
{
  return m_1.ptr == m_2.ptr and m_1.len == m_2.len;
}
  
}


TEST(SerializeMatches, serialize)
{
  std::stringstream ss;
  std::vector<rlz::match> matches {{
    {0UL, 1UL},
    {2UL, 3UL},
    {4UL, 5UL}
  }};

  rlz::serialize::matches::store(ss, matches.begin(), matches.end());

  std::stringstream is { ss.str() };

  auto recovered = rlz::serialize::matches::load(is);

  ASSERT_EQ(matches, recovered);
}