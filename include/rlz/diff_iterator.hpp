#pragma once

#include <boost/iterator/iterator_facade.hpp>

namespace rlz { namespace utils {

template <typename Iter, typename T>
class iterator_diff : public boost::iterator_facade<iterator_diff<Iter, T>, T, boost::forward_traversal_tag, T> {
private:
  Iter current;
  T last;
//  using Difference = typename boost::iterator_facade<iterator_diff<Iter, T>, T, boost::forward_traversal_tag, T>::Difference;
  using Difference = std::ptrdiff_t;

public:

  iterator_diff(T first, Iter ptr) 
    : current(ptr), last(first)
  {

  }

  iterator_diff(Iter ptr)
    : iterator_diff(T{}, ptr)
  { 
  }

  iterator_diff(Iter begin, Iter ptr)
    : iterator_diff((begin == ptr) ? T{} : *std::next(begin, std::distance(begin, ptr) - 1), ptr)
  {
  }

  static iterator_diff<Iter, T> end_iterator(Iter end_ptr)
  {
    return iterator_diff<Iter, T>{ end_ptr };
  }

private:
  friend class boost::iterator_core_access;
  
  bool equal(const iterator_diff &other) const { return current == other.current; }
  
  T dereference() const { return *current - last; }
  
  void increment()
  {
    last = *current++; 
  }
  
  void advance(Difference n) { std::advance(current, n - 1); increment(); }
  
  void decrement() { advance(-1); }
  
  size_t distance_to(const iterator_diff &other) const { return std::distance(current, other.current); }

};

}}