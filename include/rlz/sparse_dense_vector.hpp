#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <vector>

#include "bit_vectors.hpp"
#include "check_random.hpp"
#include "sdsl_extensions/int_vector.hpp"
#include "type_name.hpp"

#include <boost/iterator/iterator_facade.hpp>

namespace rlz { namespace vectors {

template <typename ChunkSize>
class sd_bitvector {
private:
  using SparseRank = rlz::vectors::BitVector<rlz::vectors::sparse, rlz::vectors::algorithms::rank>;
  SparseRank chunk_indicator;
  sdsl::extensions::int_vector<ChunkSize::value()>  bit_vectors;
  size_t length_;

  template <typename It>
  void build(It begin, It end, size_t size)
  {
    size_t frontier = 0UL;
    std::vector<size_t> chunk_set;
    std::vector<size_t> vectors;
    for (auto it = begin; it != end; ++it) {
      auto val = *it;
      if (val >= frontier) {
        auto last_chunk = val / ChunkSize::value();
        chunk_set.push_back(last_chunk);
        frontier = (last_chunk + 1) * ChunkSize::value();
        vectors.push_back(0UL);
      }
      auto &last_bv   = vectors.back();
      last_bv        |= 1UL << (val - (frontier - ChunkSize::value()));
    }
    auto chunks     = (size + ChunkSize::value() - 1) / ChunkSize::value();
    assert(chunk_set.size() <= chunks);
    chunk_indicator = SparseRank(std::make_tuple(chunk_set.begin(), chunk_set.end(), chunks));
    bit_vectors     = sdsl::extensions::int_vector<ChunkSize::value()>(vectors.size());
    std::copy(vectors.begin(), vectors.end(), bit_vectors.begin());
  }

public:

  sd_bitvector() { }

  template <typename It>
  sd_bitvector(It begin, It end, size_t size) : length_(size)
  {
    build(begin, end, size);

    // std::cout << "=== ";
    // for (auto i : chunk_indicator) {
    //   std::cout << i << " ";
    // }
    // std::cout << std::endl;
    // for (auto i = 0U; i < bit_vectors.size(); ++i) {
    //   std::cout << "=== " << i << ": " << std::hex << std::uppercase << ((unsigned int)bit_vectors[i]) << std::dec << "\n";
    // }
    // std::cout << std::endl;
  }

  class iterator;
  CHECK_TYPE_RANDOM(iterator, "sd_bitvector::iterator is not random access");

  iterator begin() const
  {
    return iterator(
      this, 0UL, 
      chunk_indicator.begin(), 
      bit_vectors.begin()
    );
  }
  iterator end() const
  {
    return iterator(
      this, length(),
      chunk_indicator.end(),
      bit_vectors.end()
    );
  }
  iterator from(size_t idx) const
  {
    auto block_id = idx / ChunkSize::value();
    assert(block_id < chunk_indicator.size());
    return iterator(
      this, idx,
      std::next(chunk_indicator.begin(), block_id),
      std::next(bit_vectors.begin(), chunk_indicator.rank_1(block_id))
    );
  }
  const size_t   length() const { return length_; } 
  bool operator[](size_t idx) const { assert(idx < length()); return *from(idx); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += chunk_indicator.serialize(out, child, "chunk_indicator");
    written_bytes += bit_vectors.serialize(out, child, "bit_vectors");
    written_bytes += sdsl::write_member(length_, out, child, "length");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;    
  }

  void load(std::istream& in)
  {
    chunk_indicator.load(in);
    bit_vectors.load(in);
    sdsl::read_member(length_, in);
  }

};

template <typename ChunkSize>
class sd_bitvector<ChunkSize>::iterator
  : public boost::iterator_facade<sd_bitvector<ChunkSize>::iterator, const bool, boost::random_access_traversal_tag>
{
public:
  using difference_type = ptrdiff_t;
  friend class sd_bitvector<ChunkSize>;
private:
  using Parent          = sd_bitvector<ChunkSize>;
  using indicator_it    = SparseRank::iterator;
  using bv_it           = typename sdsl::extensions::int_vector<ChunkSize::value()>::const_iterator;
  static constexpr std::array<bool, 2> true_false {{ false, true }};
  const Parent       *ptr;
  size_t       idx;
  indicator_it chunk_it;
  bv_it        bit_vectors;

  size_t displacement() const
  {
    return idx % ChunkSize::value();
  }

  iterator(const Parent *ptr, size_t idx, indicator_it chunk_it, bv_it bit_vectors)
    : ptr(ptr), idx(idx), chunk_it(chunk_it), bit_vectors(bit_vectors)
  { }

  friend class boost::iterator_core_access;

  const bool &dereference() const
  {
    // std::cout << "*** IDX  = " << idx << "\n"
    //           << "*** Head = " << *chunk_it << "\n"
    //           << "*** DISP = " << displacement() << "\n"
    //           << "*** BV   = " << std::hex << std::uppercase << ((unsigned int)(*bit_vectors)) << std::dec << "\n"
    //           << "*** Bit  = " << ((bool) (((*bit_vectors) >> displacement()) & 0x1)) << std::endl;
    return true_false[(*chunk_it) and (((*bit_vectors) >> displacement()) & 0x1)];
  }

  void increment()
  {
    if ((++idx % ChunkSize::value()) == 0 and *chunk_it++ == 1) {
      ++bit_vectors;
      // std::cout << "*** " << idx << ": " << std::hex << std::uppercase << ((unsigned int)(*bit_vectors)) << std::endl;
    }
  }

  bool equal(const iterator &other) const { return idx == other.idx; }

  void decrement()
  {
    *this = ptr->from(idx); // indicator_it does not support operator--
  }

  difference_type distance_to(const iterator &other) const { return other.idx - idx; }

  void advance(difference_type n)
  {
    switch (n) {
      case 1:  increment(); break;
      default: {
        // Does it cross boundaries?
        std::int64_t dis = displacement();
        if ((n < 0) or (static_cast<size_t>(dis + n) >= ChunkSize::value()) or (dis + n < 0)) {
          // Yes -- don't bother, just get a new one
          *this = ptr->from(idx + n);
        } else {
          // No -- just change idx
          idx += n;
        }        
      }
    }
  }
};

template <typename ChunkSize>
constexpr std::array<bool, 2> sd_bitvector<ChunkSize>::iterator::true_false;

}}