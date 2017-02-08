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

#include "lcp/coordinator.hpp"

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

  template <typename SymbolIt>
  class agnostic_builder : public rlz::build::agnostic_observer<Alphabet, SymbolIt> {
    using Symbol = typename Alphabet::Symbol;
    using lsk_t  = literal_split_keeper<Alphabet, Prefix>;
    enum State { FILLING, FILLED, FINISHED };
    // WARNING: not very size-efficient, to be changed.
    State state;
    size_t literal_length;
    std::vector<size_t> lit_lengths;
    std::vector<Symbol> literals;

  public:

    agnostic_builder() : state(FILLING), literal_length(0U) { }

    // returns maximum prefix length which is OK to split.

    std::tuple<std::size_t, std::size_t> can_split(
      std::size_t, std::ptrdiff_t, std::size_t copy_len, std::size_t junk_len
    ) override
    {
      assert(state == FILLING);
      return std::make_tuple(copy_len, std::min<std::size_t>(junk_len, max_literal_length));
    }

    // Returns true if splitting here allocates an another block.
    bool split_as_block(
      std::size_t, std::ptrdiff_t, std::size_t, std::size_t
    ) override
    { 
      assert(state == FILLING); return false; 
    }

    // Splits at given phrase.
    void split(
      std::size_t, std::ptrdiff_t, std::size_t, std::size_t junk_len,
      const SymbolIt bytes, bool
    ) override
    {
      assert(state == FILLING);
      literal_length += junk_len;
      lit_lengths.push_back(junk_len);
      literals.insert(literals.end(), bytes, std::next(bytes, junk_len));
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

  template <typename SymbolIt>
  class builder : public rlz::build::forward_observer_adapter<
    literal_split_keeper<Alphabet, Prefix>::agnostic_builder<SymbolIt>,
    Alphabet,
    SymbolIt
  >
  {
  private:
    using LK         = literal_split_keeper<Alphabet, Prefix>;
    using ParentType = LK::agnostic_builder<SymbolIt>;

    ParentType wrapped;
  protected:
    ParentType *get_wrapped() override
    {
      return &wrapped;
    }
  public:
    LK get()
    {
      return wrapped.get();
    }
  };

  template <typename SymbolIt>
  class lcp_builder : public rlz::lcp::build::lcp_observer_adapter<
    literal_split_keeper<Alphabet, Prefix>::agnostic_builder<SymbolIt>,
    Alphabet,
    SymbolIt
  >
  {
  private:
    using LK         = literal_split_keeper<Alphabet, Prefix>;
    using ParentType = LK::agnostic_builder<SymbolIt>;

    ParentType wrapped;
  protected:
    ParentType *get_wrapped() override
    {
      return &wrapped;
    }
  public:
    LK get()
    {
      return wrapped.get();
    }
  };
};

template <typename Alphabet, typename Prefix>
constexpr size_t literal_split_keeper<Alphabet, Prefix>::max_literal_length;

}
