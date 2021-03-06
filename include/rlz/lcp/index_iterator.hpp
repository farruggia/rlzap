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
        impl::construct{}.get<RefIt>(), 0UL, 0UL, 0UL, T{}, T{}
      )
  { }

  iterator(
    LitIt lit_begin, LitIt lit_end, RefIt ref_begin, std::size_t phrase_start, std::size_t pos, std::size_t copy_len,
    T last_lit, T prev_ref
  )
    : lit_begin(lit_begin), lit_end(lit_end), ref_begin(ref_begin),
      lits(std::distance(lit_begin, lit_end)), copy_len(copy_len),
      last_lit(last_lit), prev_ref(prev_ref),
      phrase_start(phrase_start), pos(pos),
      ref_it(ref_begin)
  {
    assert(pos <= lits + copy_len);
    if (pos > lits) {
      std::advance(ref_it, pos - lits);
    }
  }

  iterator(RefIt ref_begin, std::size_t end_pos, std::size_t copy_len)
    : iterator(LitIt{}, LitIt{}, ref_begin, end_pos, copy_len, T{}, T{})
  { }

  std::size_t position() const
  {
    return phrase_start + pos;
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
    if (pos > lits) {
      ++ref_it;
    }
  }

  void decrement()
  {
    assert(pos > 1);
    if (pos > lits) {
      --ref_it;
    }
    --pos;
  }

  void advance(diff_t n)
  {
    assert(
      (n <= 0 and pos >= static_cast<std::size_t>(-n)) or
      (n > 0  and pos + n <= lits + copy_len)
    );
    diff_t old_displace = std::max(pos, lits);
    pos += n;
    diff_t displace = std::max(pos, lits);
    std::advance(ref_it, displace - old_displace);
  }

  diff_t distance_to(iterator<T, LitIt, RefIt> const &other) const
  {
    return static_cast<diff_t>(other.position()) - static_cast<diff_t>(position());
  }

  const T &dereference() const
  {
    if (pos < lits) {
      Val = *std::next(lit_begin, pos);
    } else {
      Val = last_lit + *ref_it - prev_ref;
    }
    return Val;
  }

  LitIt lit_begin, lit_end;
  RefIt ref_begin;
  size_t lits, copy_len;
  T last_lit, prev_ref;
  std::size_t phrase_start, pos;
  RefIt ref_it;
};

}}
