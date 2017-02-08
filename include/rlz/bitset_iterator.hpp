#pragma once

#include <algorithm>
#include <cstdint>

#include <sdsl/bits.hpp>

#include "impl/bitset_iterator.hpp"

#include <boost/iterator/iterator_facade.hpp>

namespace rlz { namespace vectors {

////////////////////////////////// GENERIC BITSET SUPPORT //////////////////////////////////
template <typename BitVector, typename NextPolicy = impl::next_spin>
class bit_set_iterator 
  : public boost::iterator_facade<
      bit_set_iterator<BitVector>, size_t, boost::forward_traversal_tag, size_t
    >
{
  const BitVector     *bv;              // bitvector implementation
  size_t              next;             // Highest index which we know we don't have to report next
  size_t              end;              // one-past-end index of bv
  size_t              bits_in_bitmap;   // length of bit_map
  std::uint64_t       bit_map;          // bits following next in the bit vector
  NextPolicy          get_next;         // Tells how many positions we should skip on a row of 0s

  void reload_bitmap()
  {
    assert(next != end);
    bits_in_bitmap  = std::min(64UL, end - next - 1);
    bit_map         = bv->get_int(next + 1, bits_in_bitmap);
  }

public:

  struct position {
    size_t value;

    explicit position(size_t value) : value(value) { }

    operator size_t() { return value; }
  };

  bit_set_iterator(const BitVector *bv_, position pos, NextPolicy get_next = NextPolicy{})
    : bv(bv_),
      next(pos),
      end(bv->size()),
      bits_in_bitmap(pos == end ? 0UL : std::min(64UL - ((next + 1) & 0x40), end - next - 1)),
      bit_map(bv_->get_int(next + 1, bits_in_bitmap)), // Apparently it's safe to ask for 0 bits
      get_next(get_next)
  { }

  // Index by default, position explicitly requested, template to avoid compilation failure
  bit_set_iterator(const BitVector *bv_, size_t index, NextPolicy get_next)
    : bit_set_iterator(bv_, position(bv_->select_1(index + 1)), get_next)
  {

  }
private:
  friend class boost::iterator_core_access;

  bool equal(const bit_set_iterator<BitVector>& other) const
  {
    return next == other.next;
  }

  void increment()
  {
    if (bits_in_bitmap == 0) {
      next = end;
      return;
    }
    if (bit_map == 0U) {
      next += bits_in_bitmap;
      reload_bitmap();
      while (bit_map == 0U) {
        next = get_next(next, bits_in_bitmap);
        reload_bitmap();
      }
    }
    
    auto to_skip = sdsl::bits::lo(bit_map) + 1;
    bit_map    >>= to_skip;
    next        += to_skip;
    bits_in_bitmap -= to_skip;
    if (bits_in_bitmap == 0 and next != end) {
      reload_bitmap();
    }
  }

  size_t dereference() const { return next; }

};

}}