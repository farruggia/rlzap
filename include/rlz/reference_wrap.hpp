#pragma once

#include <iterator>

#include <boost/iterator/iterator_adaptor.hpp>


namespace rlz { namespace impl {

template <typename OrigIter, typename T>
class const_wrap_iterator
  : public boost::iterator_adaptor<
      const_wrap_iterator<OrigIter, T>,
      OrigIter,
      T,
      std::random_access_iterator_tag,
      T
    >
{
  friend class boost::iterator_core_access;
  T dereference() const { return *(this->base_reference()); }
public:
  using boost::iterator_adaptor<const_wrap_iterator<OrigIter, T>, OrigIter, T, std::random_access_iterator_tag, T>::iterator_adaptor;
};

}}
