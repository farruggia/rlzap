#pragma once

#include <cassert>
#include <functional>
#include <iterator>

namespace rlz { namespace classic {
struct parser {
  template <
    typename MatchIt, typename LiteralEvt, typename CopyEvt, 
    typename EndEvt = std::function<void()>
  >
  void operator() (
    MatchIt match_begin, MatchIt match_end, 
    size_t,
    LiteralEvt literal_func, CopyEvt copy_func, EndEvt end_evt
  ) const
  {
    for (auto m_it = match_begin; m_it != match_end; ) {
      if (m_it->len > 1) {
        auto advance = std::min<size_t>(m_it->len, std::distance(m_it, match_end) - 1U);
        copy_func(std::distance(match_begin, m_it), m_it->ptr, advance);
        std::advance(m_it, advance);
      }
      literal_func(std::distance(match_begin, m_it++), 1UL);
    }
    end_evt();
  }
};

} }
