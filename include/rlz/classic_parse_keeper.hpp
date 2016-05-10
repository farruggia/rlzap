#pragma once

#include "bit_vectors.hpp"
#include "build_coordinator.hpp"
#include "int_vector.hpp"
#include "impl/parse_keeper.hpp"

#include <boost/iterator/transform_iterator.hpp>

namespace rlz { namespace classic {

template <
  typename Alphabet,
  typename PtrSize         = values::Size<32UL>,
  typename PhraseBVRep     = vectors::dense, //vectors::DenseRank,
  typename SubphraseBVRep  = vectors::sparse //vectors::SparseRankSelect
>
class parse_keeper {
private:
  using PhraseBV    = vectors::BitVector<PhraseBVRep, vectors::algorithms::rank>;
  using SubphraseBV = vectors::BitVector<SubphraseBVRep, vectors::algorithms::rank, vectors::algorithms::select, vectors::algorithms::bitset>;
  using PtrsIter    = typename ds::int_vector<PtrSize::value()>::const_iterator;  
  PhraseBV                            pbv;
  SubphraseBV                         sbv;
  ds::int_vector<PtrSize::value()>    ptrs;
public:
  using ptr_type = std::int64_t;
  static constexpr const size_t ptr_size = PtrSize::value();
  static constexpr const size_t delta_bits = 0UL;

  class builder;

  class iterator;

  // Returns (phrase, subphrase)
  std::tuple<size_t, size_t> phrase_subphrase(size_t position) const
  {
    // First phrase is NOT marked
    auto sub = sbv.rank_1(position);
    auto phr = pbv.rank_1(sub);
    return std::make_tuple(phr, sub);
  }

  // Returns the differential pointer associated to the (phrase, subphrase) couple
  ptr_type get_ptr(size_t phrase, size_t) const
  {
    // Remember: int_vectors return references which translate unsigned integers into
    // signed ones at every evaluation: better convert them straightly to ptr_type upon return
    // to avoid performance issues
    return ptrs[phrase];
  }

  size_t start_subphrase(size_t subphrase) const
  {
    // sbv has a sentinel 1 past the end of the bitstream, so it's safe to get the length
    // of a match by querying the beginning of the next subphrase
    return subphrase != 0 ? sbv.select_1(subphrase) + 1UL : 0;
  }

  iterator get_iterator(size_t phrase_index, size_t subphrase_idx) const
  {
    return iterator(this, phrase_index, subphrase_idx);
  }

  iterator get_iterator_begin() const
  {
    return get_iterator(0UL, 0UL);
  }

  iterator get_iterator_end() const
  {
    return iterator(this);
  }

  // Returns the document length (in symbols - be it bases or characters)
  size_t length() const
  {
    return sbv.size() - 1; // -1 because of the sentinel
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += pbv.serialize(out, child, "Block BV");
    written_bytes += sbv.serialize(out, child, "Sub-block BV");
    written_bytes += ptrs.serialize(out, child, "Block pointers");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    pbv.load(in);
    sbv.load(in);
    ptrs.load(in);
  }
};

template <typename Alphabet, typename PtrSize, typename PhraseBVRep, typename SubphraseBVRep>
class parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>::iterator
  : public boost::iterator_facade<
        parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>::iterator,
        std::tuple<size_t, std::int64_t, size_t>,
        boost::forward_traversal_tag,
        std::tuple<size_t, std::int64_t, size_t>
    >
{
private:
  using Parent      = parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>;

  const Parent *ptr;

  struct Increment {
    size_t operator()(const size_t &i) const {
      return i + 1UL;
    }
  };

  // Ugly and not really generic (doesn't work when constness is not embedding into type), must find an another way to do it
  using PhraseIter  = type_utils::RemoveQualifiers<decltype(ptr->pbv.begin())>;
  using RawSubIter = type_utils::RemoveQualifiers<decltype(ptr->sbv.get_bitset(0U))>;
  using SubIter    = boost::transform_iterator<Increment, RawSubIter>;
  using PtrsIter    = typename parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>::PtrsIter;


  PhraseIter  phrase_iter;
  SubIter     sub_iter;
  PtrsIter    ptr_iter;
  size_t      phrase_start;
  size_t      phrase_len;
  using PtrValue    = std::uint64_t;
  PtrValue    phrase_ptr;

  void update_subphrase()
  {
    phrase_start = *sub_iter;
    phrase_len   = *++sub_iter - phrase_start;
  }

  void update_ptr()
  {
    phrase_ptr = *ptr_iter;
  }

public:
  iterator(const Parent *ptr, size_t phrase_index, size_t subphrase_index) // Normal iterator
    : ptr(ptr),
      phrase_iter(std::next(ptr->pbv.begin(), subphrase_index)),
      // Messy index to compute length first phrase (one before, update in ctor body) and account special case sub_idx == 0
      sub_iter(boost::make_transform_iterator<Increment>(ptr->sbv.get_bitset((subphrase_index > 0) ? (subphrase_index - 1) : 0))),
      ptr_iter(std::next(ptr->ptrs.begin(), phrase_index)),
      phrase_start(subphrase_index == 0 ? 0UL : *sub_iter),
      // So it's correct for subphrase_index == 0
      phrase_len(*sub_iter)
  {
    if (subphrase_index > 0) {
      update_subphrase();
    }
    update_ptr();
  }

  iterator(const Parent *ptr) // End iterator
    : ptr(ptr),
      phrase_iter(ptr->pbv.end()),
      sub_iter(ptr->sbv.get_bitset_end()),
      ptr_iter(ptr->ptrs.end())
  { }


private:
  friend class boost::iterator_core_access;

  bool equal(const iterator& other) const
  {
      return sub_iter == other.sub_iter;
  }

  void increment()
  {
    if (*phrase_iter++) {
      ++ptr_iter;
    }
    update_subphrase();
    update_ptr();
  }

  std::tuple<size_t, std::int64_t, size_t> dereference() const
  {
    return std::make_tuple(phrase_start, phrase_ptr, phrase_len);
  }
};

template <typename Alphabet, typename PtrSize, typename PhraseBVRep, typename SubphraseBVRep>
class parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>::builder : public rlz::build::observer<Alphabet> {
private:
  using Symbol = typename Alphabet::Symbol;
  enum State { FILLING, FILLED, FINISHED };
public:
  static constexpr std::int64_t ptr_high  = impl::field_limits<PtrSize::value()>::high();
  static constexpr std::int64_t ptr_low   = impl::field_limits<PtrSize::value()>::low();
private:
  using PK = parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>;

  PK pk_build;

  size_t source_len;

  std::int64_t block_ptr;

  std::vector<size_t> sub_blocks;
  sdsl::bit_vector phrase_indicator;

  State state;
  bool first_phrase;

  std::int64_t compute_delta(std::uint64_t source, std::uint64_t target)
  {
    return static_cast<std::int64_t>(target) - static_cast<std::int64_t>(source);
  }
public:

  builder() 
    : source_len(0U), block_ptr(-1), state(FILLING), first_phrase(true)
  {
  }

  // returns maximum prefix length which is OK to split.
  size_t can_split(size_t, size_t, size_t len, size_t junk_len, const Symbol *) override
  {
    assert(state == FILLING);
    assert(junk_len == 1);
    return len + junk_len;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t source, size_t target, size_t len, size_t, const Symbol *) override
  {
    assert(state == FILLING);
    auto delta = compute_delta(source, target);
    return first_phrase or ((len > 0) and (delta != block_ptr));
  }


  // Splits at given phrase.
  void split(size_t source, size_t target, size_t len, size_t junk_len, const Symbol *bytes, bool new_block) override
  {
    assert(state == FILLING);
    block_ptr = compute_delta(source, target);

    // Set bit-vectors
    sub_blocks.push_back(source);
    if (phrase_indicator.size() < sub_blocks.size()) {
      phrase_indicator.resize(sub_blocks.size() * 2);
    }
    if (sub_blocks.size() > 1) {
      phrase_indicator[sub_blocks.size() - 2] = new_block;
    }

    // Saves ptr and delta
    if (new_block) {
      pk_build.ptrs.push_back(block_ptr);
    }
    source_len   = source + len + junk_len;
    first_phrase = false;
  }

  // Parsing finished
  void finish() override
  {
    assert(state == FILLING);
    // Resize phrase indicator bitvector and corrects setting first sub-block as block begin
    phrase_indicator.resize(sub_blocks.size());
    phrase_indicator[sub_blocks.size() - 1] = 1; // Sentinel value
    // phrase_indicator[0] = 0;

    // Sentinel value
    sub_blocks.push_back(source_len);

    // One-less for every sub-block
    for (auto &i : sub_blocks) {
      --i;
    }

    state = FILLED;
  }

  PK get()
  {
    assert(state == FILLED);
    // assert(sub_blocks.front() == 0);
    pk_build.pbv = PhraseBV(std::move(phrase_indicator));
    pk_build.sbv = SubphraseBV(std::make_tuple(std::next(sub_blocks.begin()), sub_blocks.end(), source_len + 1)); // +1 for sentinel
    // assert(pk_build.pbv[0] == 0);
    state = FINISHED;
    return std::move(pk_build);
  }
};

} }