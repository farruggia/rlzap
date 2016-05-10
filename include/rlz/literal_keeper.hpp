#pragma once

#include <sdsl/int_vector.hpp>
#include <sdsl/util.hpp>

#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/zip_iterator.hpp>

#include "bit_vectors.hpp"
#include "build_coordinator.hpp"
#include "check_random.hpp"
#include "integer_type.hpp"
#include "prefix_sum.hpp"
#include "sparse_dense_vector.hpp"
#include "type_name.hpp"

#include <array>
#include <cassert>
#include <iterator>
#include <vector>

namespace rlz {

template <typename Alphabet,  typename Prefix = rlz::prefix::cumulative<vectors::sparse, values::Size<4U>>>
class literal_split_keeper {
private:
  using Packer = typename Alphabet::Packer;
  Prefix lit_prefix;
  Packer pack;
public:
  static constexpr size_t max_literal_length = Prefix::max_length;
  using lit_iter = typename Prefix::iterator;
  using iterator = typename Packer::iterator;

  CHECK_TYPE_RANDOM(iterator, "literal_split_keeper::iterator is not random access");

  size_t literal_length(size_t subphrase) const
  {
    return lit_prefix.access(subphrase);
  }

  iterator literal_access(size_t subphrase) const
  {
    auto lit_idx = subphrase == 0 ? 0 : lit_prefix.prefix(subphrase - 1);
    return pack.from(lit_idx);
  }

  lit_iter get_iterator(size_t subphrase) const
  {
    return lit_prefix.get_iterator(subphrase);
  }

  lit_iter get_iterator_end() const
  {
    return lit_prefix.get_iterator_end();
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += lit_prefix.serialize(out, child, "Literal lengths");
    written_bytes += pack.serialize(out, child, "Literals");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    lit_prefix.load(in);
    pack.load(in);
  }

  class builder;
};

template <typename Alphabet, typename Prefix>
constexpr size_t literal_split_keeper<Alphabet, Prefix>::max_literal_length;

template <typename Alphabet, typename Prefix>
class literal_split_keeper<Alphabet, Prefix>::builder : public build::observer<Alphabet> {
  using Symbol = typename Alphabet::Symbol;
  using lsk_t  = literal_split_keeper<Alphabet, Prefix>;
  enum State { FILLING, FILLED, FINISHED };
  // WARNING: not very size-efficient, to be changed.
  State state;
  size_t literal_length;
  std::vector<size_t> lit_lengths;
  std::vector<Symbol> literals;

public:

  builder() : state(FILLING), literal_length(0U) { }

  // returns maximum prefix length which is OK to split.
  size_t can_split(size_t, size_t, size_t len, size_t junk_len, const Symbol *bytes) override
  {
    assert(state == FILLING);
    return len + std::min(junk_len, max_literal_length);
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t, size_t, size_t, size_t, const Symbol *) override
  { 
    assert(state == FILLING); return false; 
  }

  // Splits at given phrase.
  void split(size_t, size_t, size_t, size_t junk_len, const Symbol *bytes, bool) override
  {
    assert(state == FILLING);
    literal_length += junk_len;
    lit_lengths.push_back(junk_len);
    literals.insert(literals.end(), bytes, bytes + junk_len);
  }

  // Parsing finished
  void finish() override
  {
    assert(state == FILLING);
    state = FILLED;
  }

  lsk_t get()
  {
    assert(state == FILLED);
    lsk_t to_ret;

    // Manage cumulative and lit_lengths
    to_ret.lit_prefix   = Prefix(lit_lengths.begin(), lit_lengths.end());

    // Manage junks
    using Packer = typename Alphabet::Packer;
    to_ret.pack         = Packer(literals.begin(), literals.end());

    state = FINISHED;
    return to_ret;
  }
};


}
