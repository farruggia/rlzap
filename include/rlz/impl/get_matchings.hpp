#pragma once

#include "../match.hpp"

#include <sais.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace rlz {
namespace impl {
/* Given two files F1 and F2, concatenates them into F1 + 0 + F2 + 0.
 * Return:
 *  1st element in tuple: concatenation
 *  2nd: length of F1
 *  3th: length of F2.
 */
template <typename Alphabet, typename Dump_1, typename Dump_2>
std::tuple<size_t, size_t>
read_joined(
  Dump_1 &d_1, Dump_2 &d_2,
  std::vector<typename Alphabet::Symbol> &T
)
{
  using namespace std;
  using Symbol = typename Alphabet::Symbol;

  // constexpr const Symbol separator = std::numeric_limits<Symbol>::lowest();
  constexpr const Symbol separator = Symbol{}; //std::numeric_limits<Symbol>::lowest();

  d_1.dump(T);
  auto len_1 = T.size();
  T.push_back(separator);
  d_2.dump(T);
  auto len_2 = T.size() - len_1 - 1;
  T.push_back(separator);
  T.shrink_to_fit();
  return std::make_tuple(len_1, len_2);
}

template <typename SaIt, typename LcpIt>
std::vector<rlz::match> longest_match(SaIt sa_it, LcpIt lcp_it, size_t ref_len, size_t in_len)
{
  using rlz::match;
  const auto n        = ref_len + in_len + 1;
  const auto sa_beg   = sa_it;
  const auto sa_end   = std::next(sa_it, n + 1);
  std::vector<match> M(in_len);

  match cur;

  auto update = [&] (size_t pos, const match &m)
  {
    if (M[pos].len < m.len) {
      // std::cout << "MAP TO: " << m.ptr
      //           << " @ " << m.len
      //           << std::flush;

      // assert(pos + m.len <= in_len);
      assert(m.ptr + m.len <= ref_len);
      M[pos] = m;
    } else {
      // std::cout << "BETTER MATCH AT " << M[pos].ptr
      //           << " @ " << M[pos].len
      //           << std::flush;
    }
  };

  auto process = [&cur, &in_len, &update] (SaIt sa_it, LcpIt lcp_it) -> void {
    auto pos = *sa_it;
    cur.len = std::min<size_t>(cur.len, *lcp_it);

    // std::cout << "--- "
    //           << std::setw(3) << pos << " "
    //           << std::flush;

    if (pos < in_len) {
      update(pos, cur);
    } else if (pos > in_len) {
      cur = match(pos - in_len - 1, in_len);
      // std::cout << "SET CURRENT TO " << cur.ptr
      //           << " @ " << in_len << std::flush;
    }
    // else {
      // std::cout << "--- SEPARATOR ---" << std::flush;
    // }
    // std::cout << std::endl;
  };

  // std::cout << "REF LEN: " << ref_len << "\n"
  //           << "INP LENL " << in_len << "\n"
  //           << "--- FORWARD SCAN ---" << std::endl;

  // Forward scan
  for (; sa_it != sa_end; ++sa_it, ++lcp_it) {
    process(sa_it, lcp_it);
  }

  // std::cout << "--- BACKWARD SCAN ---" << std::endl;

  // Special processing for first backward element
  --sa_it;
  cur = (*sa_it < in_len) ? match() : match(*sa_it - in_len - 1, in_len);
  --sa_it;
  --lcp_it;

  for (; sa_it >= sa_beg; --sa_it, --lcp_it) {
    process(sa_it, lcp_it);
  }

  // Fix match lengths
  for (size_t i = 0U; i < M.size(); ++i) {
    M[i].len = std::min<size_t>(M[i].len, in_len - i);
  }
  return M;
}

}
  
}