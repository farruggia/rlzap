#pragma once

#include <boost/iterator/iterator_facade.hpp>
#include <type_traits>

namespace rlz {

namespace detail {
  template <typename T>
  using SignedType = typename std::make_signed<typename std::remove_reference<T>::type>::type;
}

template <
  typename RandomIt,
  typename Value = detail::SignedType<decltype(*RandomIt{})>
>
class differential_iterator : public boost::iterator_facade<
                                differential_iterator<RandomIt>,
                                Value const,
                                boost::forward_traversal_tag,
                                Value const
                              >
{
  Value previous;
  RandomIt it;
  friend class boost::iterator_core_access;
public:
  differential_iterator(RandomIt it) : previous(Value{}), it(it) { }

  void increment()
  {
    previous = *it;
    std::advance(it, 1); 
  }

  bool equal(differential_iterator<RandomIt, Value> const& other) const
  {
      return this->it == other.it;
  }

  Value dereference() const
  {
    return *it - previous;
  }
};
}