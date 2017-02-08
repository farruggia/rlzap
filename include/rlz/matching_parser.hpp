#pragma once

#include <cstdint>
#include <functional>

namespace rlz {

template <typename Strategy, typename MatchIt>
class matching_strategy {
  Strategy strategy;
  MatchIt begin;
  MatchIt end;
  std::size_t reference_size;
public:

  matching_strategy() { }

  matching_strategy(Strategy strategy, MatchIt begin, MatchIt end, std::size_t reference_size)
    : strategy(strategy), begin(begin), end(end), reference_size(reference_size)
  { }

  template <typename LiteralEvt, typename CopyEvt, typename EndEvt = std::function<void()>>
  void operator()(LiteralEvt literal_func, CopyEvt copy_func, EndEvt end_evt)
  {
    strategy(begin, end, reference_size, literal_func, copy_func, end_evt);
  }

};

}