#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <unordered_map>

#include <boost/iterator/iterator_facade.hpp>

#include <sdsl/bit_vectors.hpp>
#include <sdsl/bits.hpp>

#include "sdsl_extensions/sd_vector.hpp"

namespace rlz { namespace vectors { namespace algorithms { namespace bitsets {

namespace detail {

class dense_bv_next {
  const std::unordered_map<size_t, size_t> *map;
public:

  dense_bv_next() { }

  dense_bv_next(const std::unordered_map<size_t, size_t> *map) : map(map) { }

  size_t operator()(size_t pos, size_t) const
  {
    assert(map->find(pos) != map->end());
    return (map->find(pos))->second;
  }
};

struct dense_bv_next_builder {
  mutable size_t first;
  mutable size_t last;

  dense_bv_next_builder() : first(0UL), last(0UL) { }

  size_t operator()(size_t pos, size_t bitset_len) const
  {
    first = (first == 0) ? pos : first;
    last  = pos + bitset_len;
    return last;
  }
};

struct next_spin {

  size_t operator()(size_t pos, size_t bits_in_bitmap) const
  {
    return pos + bits_in_bitmap;
  }

};


template <typename BitVector, typename NextPolicy>
class bit_set_iterator 
  : public boost::iterator_facade<
      bit_set_iterator<BitVector, NextPolicy>, size_t, boost::forward_traversal_tag, size_t
    >
{
  const BitVector     *bv;              // bitvector implementation
  size_t              next;             // Highest index which we know we don't have to report next
  size_t              end;              // one-past-end index of bv
  size_t              bits_in_bitmap;   // length of bit_map
  std::uint64_t       bit_map;          // bits following next in the bit vector
  const NextPolicy    *get_next;        // Tells how many positions we should skip on a row of 0s

  void reload_bitmap()
  {
    assert(next != end);
    bits_in_bitmap  = std::min(64UL, end - next - 1);
    bit_map         = bv->get_int(next + 1, bits_in_bitmap);
    // std::bitset<64> bs(bit_map);
    // reverse(bs);
    // std::cout << "--- I: R " << std::setw(3)  << next 
    //           << " - ℓ = "   << std::setw(2)  << bits_in_bitmap 
    //           << " - B = "   << std::setw(64) << bs << std::endl;
  }

public:

  struct position {
    size_t value;

    explicit position(size_t value) : value(value) { }

    operator size_t() { return value; }
  };

  bit_set_iterator(const BitVector *bv, position pos, const NextPolicy *get_next)
    : bv(bv),
      next(pos),
      end(bv->size()),
      bits_in_bitmap(pos == end ? 0UL : std::min(64UL - ((next + 1) & 0x3F), end - next - 1)),
      bit_map(bv->get_int(next + 1, bits_in_bitmap)), // Apparently it's safe to ask for 0 bits
      get_next(get_next)
  { }

  // Index by default, position explicitly requested, template to avoid compilation failure
  bit_set_iterator(const BitVector *bv, size_t index, const NextPolicy *get_next)
    : bit_set_iterator(bv, position(bv->select_1(index + 1)), get_next)
  {

  }

  bit_set_iterator(const BitVector *bv) : bit_set_iterator(bv, position(bv->size()), nullptr) { }
private:
  friend class boost::iterator_core_access;

  bool equal(const bit_set_iterator<BitVector, NextPolicy>& other) const
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
      while (bit_map == 0U and bits_in_bitmap > 0) {
        next = (*get_next)(next, bits_in_bitmap);
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

template <typename BitVector>
std::unordered_map<size_t, size_t> compute_accelerated_map(const BitVector &bv)
{
  std::unordered_map<size_t, size_t> map;
  if (bv.size() == 0) { return map; }
  // Get iterator on hi bits
  using Iter  = bit_set_iterator<BitVector, dense_bv_next_builder>;
  dense_bv_next_builder dn_build;
  Iter iter_beg(&bv, typename Iter::position(0U), &dn_build);
  Iter iter_end(&bv, typename Iter::position(bv.size()), &dn_build);

  while (iter_beg != iter_end) {
    ++iter_beg;
    if (dn_build.last != 0UL) {
      map[dn_build.first] = dn_build.last;
      dn_build = dense_bv_next_builder{};
    }
  }
  return map;
}
  
}

template <typename BvType>
class dense {
private:
  BvType                              *bv;
  std::unordered_map<size_t, size_t>  distant_blocks;
  detail::dense_bv_next               dbn;

public:

  template <typename Bv>
  void init(Bv *bv)
  {
    distant_blocks = detail::compute_accelerated_map(bv->data());
    this->bv = bv;
  }

  void copy(BvType *bv)
  { 
    this->bv = bv;
    dbn = detail::dense_bv_next(&distant_blocks);
  }

  dense() : dbn(&distant_blocks) { }

  using iterator = detail::bit_set_iterator<BvType, detail::dense_bv_next>;

  iterator operator()(size_t index) const
  {
    return iterator(bv, index, &dbn);
  }

  iterator operator()() const
  {
    return iterator(bv);
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return 0UL;
  }

  template <typename Bv_Type>
  void load(std::istream& in, Bv_Type *bv)
  {
    init(bv);
  }
};

namespace detail {

class sparse_iterator
  : public boost::iterator_facade<sparse_iterator, size_t, boost::forward_traversal_tag, size_t>
{
private:
  using VecType   = sdsl::extensions::sd_vector<>;
  using LowIter   = sdsl::int_vector<>::const_iterator;
  using HighIter  = bit_set_iterator<sdsl::bit_vector, const detail::dense_bv_next>;
  size_t    idx;       // Index of current element
  LowIter   low_iter;  // Iterator of low bits
  HighIter  high_iter; // Iterator of high bits
  const size_t  m_lw;  // Length of low bits
  const size_t  end;   // One past maximum position in the bitvector

  size_t current_value;

  size_t compute_current()
  {
    return ((*high_iter - idx) << m_lw) | *low_iter;
  }


public:
  sparse_iterator(const VecType *ptr, const dense_bv_next *dbn, size_t idx)
    : idx(idx), 
      low_iter(std::next(ptr->m_low.begin(), idx)),
      high_iter(&(ptr->high), typename HighIter::position(ptr->m_high_1_select(idx + 1)), dbn),
      m_lw(ptr->m_wl),
      end(ptr->size()),
      current_value(compute_current())
  {
  }

  sparse_iterator(const VecType *ptr)
    : idx(ptr->m_low.size()), 
      low_iter(ptr->m_low.begin()),
      high_iter(&(ptr->high), typename HighIter::position(0UL), nullptr),
      m_lw(0U),
      end(ptr->size()),
      current_value(end)
  {

  }

private:
  friend class boost::iterator_core_access;
  bool equal(sparse_iterator const& other) const
  {
    return idx == other.idx;
  }

  void increment()
  {
    if (idx < end) {
      ++idx;
      ++low_iter;   // Advance low iterator
      ++high_iter;  // Advance high iterator
      current_value = compute_current();
    }
  }

  size_t dereference() const
  { 
    return current_value;
  }
};
}

template <typename SparseBV>
class sparse {
private:
  SparseBV                           *sbv;
  std::unordered_map<size_t, size_t> long_phrases_hi_bits;
  detail::dense_bv_next              dbn;
public:
  void init(SparseBV *bv_)
  {
    sbv = bv_;
    long_phrases_hi_bits = detail::compute_accelerated_map(sbv->data().m_high);
    dbn = detail::dense_bv_next(&long_phrases_hi_bits);
  }

  void copy(SparseBV *bv)
  {
    sbv = bv; 
    dbn = detail::dense_bv_next(&long_phrases_hi_bits);
  }
  using iterator = detail::sparse_iterator;

  iterator operator()(size_t index) const
  {
    return iterator(&(sbv->data()), &dbn, index);
  }

  iterator operator()() const
  {
    return iterator(&(sbv->data()));
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return 0UL;
  }

  template <typename Bv_Type>
  void load(std::istream& in, Bv_Type *bv)
  {
    init(bv);
  }
};

} } } }