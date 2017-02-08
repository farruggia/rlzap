#pragma once

#include <cstdint>
#include <iterator>
#include <type_traits>

#include <boost/iterator/iterator_facade.hpp>

namespace rlz { namespace lcp {

namespace impl {

// Workaround for sdsl::random_access_const_iterator not having a default constructor.
struct construct {  
  template <typename T>
  typename std::enable_if<std::is_default_constructible<T>::value, T >::type get() 
  {
    return T{};
  }

  template <typename T>
  typename  std::enable_if<
              (not std::is_default_constructible<T>::value) and std::is_constructible<T, std::nullptr_t>::value,
              T
            >::type get()
  {
    return T{nullptr};
  }
};
}

template <typename T, typename LitIt, typename RefIt>
class iterator : public boost::iterators::iterator_facade<
        iterator<T, LitIt, RefIt>,
        T,
        boost::random_access_traversal_tag,
        const T&
>
{
public:

  iterator() 
    : iterator(
        impl::construct{}.get<LitIt>(), impl::construct{}.get<LitIt>(),
        impl::construct{}.get<RefIt>(), impl::construct{}.get<RefIt>(),
        0UL, T{}, T{}
      )
  { }

  iterator(
    LitIt lit_begin, LitIt lit_end, RefIt ref_begin, RefIt ref_end, std::size_t pos,
    T last_lit, T prev_ref
  )
    : lit_begin(lit_begin), lit_end(lit_end), ref_begin(ref_begin), ref_end(ref_end),
      lits(std::distance(lit_begin, lit_end)), copy_len(std::distance(ref_begin, ref_end)),
      last_lit(last_lit), prev_ref(prev_ref), pos(pos)
  {
    assert(pos <= lits + copy_len);
  }

private:

  mutable T Val;

  friend class boost::iterator_core_access;

  using diff_t = typename boost::iterator_facade<iterator<T, LitIt, RefIt>, T, boost::bidirectional_traversal_tag>::difference_type;

  bool equal(iterator<T, LitIt, RefIt> const& other) const
  {
      return (lit_begin == other.lit_begin) and pos == other.pos;
  }

  void increment()
  {
    assert(pos < lits + copy_len);
    ++pos;
  }

  void decrement()
  {
    assert(pos > 1);
    --pos;
  }

  void advance(diff_t n)
  {
    assert(
      (n <= 0 and pos >= static_cast<std::size_t>(-n)) or 
      (n > 0  and pos + n <= lits + copy_len)
    );
    pos += n;
  }

  diff_t distance_to(iterator<T, LitIt, RefIt> const &other) const
  {
    return static_cast<diff_t>(other.pos) - static_cast<diff_t>(pos);
  }

  const T &dereference() const
  {
    if (pos < lits) {
      Val = *std::next(lit_begin, pos);
    } else {
      auto displace = pos - lits;
      Val = last_lit + *std::next(ref_begin, displace) - prev_ref;      
    }
    return Val;
  }

  LitIt lit_begin, lit_end;
  RefIt ref_begin, ref_end;
  size_t lits, copy_len;
  T last_lit, prev_ref;
  std::size_t pos;
};

}}