/* sdsl - succinct data structures library
    Copyright (C) 2009-2013 Simon Gog

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/ .
*/
/*! \file lcp_byte.hpp
    \brief lcp_byte.hpp contains a (compressed) lcp array.
    \author Simon Gog
*/
#pragma once

#include <sdsl/int_vector.hpp>
#include <sdsl/iterators.hpp>

#include "../int_vector.hpp"
#include "../type_name.hpp"

#include <algorithm> // for lower_bound
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace sdsl { namespace extensions {

namespace impl {
    
template <bool sig>
struct lcp_config {
};

template <>
struct lcp_config<true> {
    template <size_t T>
    using container = rlz::ds::int_vector<T>;

    constexpr static int high() { return 127;  }
    constexpr static int low()  { return -128; }
};

template <>
struct lcp_config<false> {
    template <size_t T>
    using container = sdsl::int_vector<T>;

    constexpr static size_t high() { return 255U; }
    constexpr static size_t low()  { return 0U; }
};

}

template<typename Symbol, size_t max_exception_log = 0UL>
class signed_lcp_byte
{
public:
  using value_type      = Symbol;
  using const_iterator  = random_access_const_iterator<signed_lcp_byte>;
  using const_reference = const value_type;
  using pointer         = const_reference*;
  using const_pointer   = pointer;
  using size_type       = size_t;
  using reference       = const_reference;
  using iterator        = const_iterator;
  using difference_type = std::ptrdiff_t;


private:
  constexpr const static size_t symbol_width = sizeof(Symbol) * 8UL;
  template <size_t T>
  using Container = typename impl::lcp_config<std::is_signed<Symbol>::value>::template container<T>;

  constexpr static const Symbol small_low = impl::lcp_config<std::is_signed<Symbol>::value>::low();
  constexpr static const Symbol small_high = impl::lcp_config<std::is_signed<Symbol>::value>::high();

  Container<8UL>            m_small_lcp;             // vector for LCP values in [-128, 127]
  Container<symbol_width>   m_big_lcp;               // vector for LCP values outside [-128, 127]
  sdsl::int_vector<max_exception_log> m_big_lcp_idx; // index of LCP entries in the LCP array
public:
  //! Default Constructor
  signed_lcp_byte() { };

  //! Constructor
  template <typename ValueIt>
  signed_lcp_byte(ValueIt begin, ValueIt end)
    : m_small_lcp(std::distance(begin, end), 0)
  {
    Symbol l = 0, big_sum = 0;

    for (size_t i = 0; i < m_small_lcp.size(); ++i) {
      l = *std::next(begin, i);
      if (l >= small_low and l < small_high) {
        m_small_lcp[i] = l;
      } else {
        m_small_lcp[i] = small_high;
        ++big_sum;
      }
    }
    m_big_lcp     = Container<symbol_width>(big_sum, 0);
    m_big_lcp_idx = sdsl::int_vector<max_exception_log>(big_sum, 0);

    for (size_t i = 0, ii = 0; i<m_small_lcp.size(); ++i) {
      l = *std::next(begin, i);
      if (l < small_low or l >= small_high) {
        m_big_lcp[ii] = l;
        m_big_lcp_idx[ii] = i;
        ++ii;
      }
    }
  }

  //! Number of elements in the instance.
  size_t size() const
  {
    return m_small_lcp.size();
  }

  //! Returns the largest size that lcp_byte can ever have.
  static size_t max_size()
  {
    return int_vector<8>::max_size();
  }

  //! Returns if the data strucutre is empty.
  bool empty() const
  {
    return m_small_lcp.empty();
  }

  //! Returns a const_iterator to the first element.
  const_iterator begin() const
  {
    return const_iterator(this, 0);
  }

  //! Returns a const_iterator to the element after the last element.
  const_iterator end() const
  {
    return const_iterator(this, size());
  }

  //! []-operator
  /*! \param i Index of the value. \f$ i \in [0..size()-1]\f$.
   * Time complexity: O(1) for small and O(log n) for large values
   */
  Symbol operator[](size_t i) const
  {
    if (m_small_lcp[i] != small_high) {
      return m_small_lcp[i];
    } else {
      size_t idx = std::lower_bound(
        m_big_lcp_idx.begin(),
        m_big_lcp_idx.end(),
        i
      ) - m_big_lcp_idx.begin();
      return m_big_lcp[idx];
    }
  }

  //! Serialize to a stream.
  size_t serialize(std::ostream& out, structure_tree_node* v=nullptr, std::string name="") const
  {
    structure_tree_node* child = structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += m_small_lcp.serialize(out, child, "small_lcp");
    written_bytes += m_big_lcp.serialize(out, child, "large_lcp");
    written_bytes += m_big_lcp_idx.serialize(out, child, "large_lcp_idx");
    structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  //! Load from a stream.
  void load(std::istream& in) {
    m_small_lcp.load(in);
    m_big_lcp.load(in);
    m_big_lcp_idx.load(in);
  }
};

template<typename Symbol, size_t max_exception_log>
constexpr const size_t signed_lcp_byte<Symbol, max_exception_log>::symbol_width;

template<typename Symbol, size_t max_exception_log>
constexpr const Symbol signed_lcp_byte<Symbol, max_exception_log>::small_low;
template<typename Symbol, size_t max_exception_log>
constexpr const Symbol signed_lcp_byte<Symbol, max_exception_log>::small_high;

} // end namespace extensions
} // end namespace sdsl
