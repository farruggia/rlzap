#pragma once

#include <array>
#include <cassert>
#include <limits>
#include <vector>

#include "bit_vectors.hpp"
#include "build_coordinator.hpp"
#include "int_vector.hpp"
#include "integer_type.hpp"
#include "impl/parse_keeper.hpp"
#include "type_name.hpp"
#include "type_utils.hpp"

#include "lcp/coordinator.hpp"

#include <boost/iterator/iterator_facade.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <sdsl/int_vector.hpp>
#include <sdsl/util.hpp>

namespace rlz {

template <
  typename Alphabet,
  typename PtrSize         = values::Size<32UL>,
  typename DiffSize        = values::Size<8UL>,
  typename PhraseBVRep     = vectors::dense, //vectors::DenseRank,
  typename SubphraseBVRep  = vectors::sparse //vectors::SparseRankSelect
>
class parse_keeper {
private:
  using PhraseBV    = vectors::BitVector<PhraseBVRep, vectors::algorithms::rank>;
  using SubphraseBV = vectors::BitVector<SubphraseBVRep, vectors::algorithms::rank, vectors::algorithms::select, vectors::algorithms::bitset>;
  using PtrsIter    = typename ds::int_vector<PtrSize::value()>::const_iterator;
  using DiffIter    = typename ds::int_vector<DiffSize::value()>::const_iterator;
  PhraseBV                            pbv;
  SubphraseBV                         sbv;
  ds::int_vector<PtrSize::value()>    ptrs;
  ds::int_vector<DiffSize::value()>   diffs;

public:

  using ptr_type = std::int64_t;
  
  static constexpr const size_t ptr_size   = PtrSize::value();
  static constexpr const size_t delta_bits = DiffSize::value();
  
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
  ptr_type get_ptr(size_t phrase, size_t subphrase) const
  {
    // Remember: int_vectors return references which translate unsigned integers into
    // signed ones at every evaluation: better convert them straightly to ptr_type upon return
    // to avoid performance issues
    ptr_type ptr = ptrs[phrase];

    // First sub-block in a block does not have a differential pointer
    if (subphrase > 0 and pbv[subphrase - 1UL] == 0) {
      ptr += diffs[subphrase - phrase]; // Remember left-sentinel
    }
    return ptr;
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
    written_bytes += diffs.serialize(out, child, "Sub-block pointers");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    pbv.load(in);
    sbv.load(in);
    ptrs.load(in);
    diffs.load(in);
  }

  struct PointerLimits {
    static constexpr std::int64_t ptr_high()
    {
      return impl::field_limits<PtrSize::value()>::high();
    }

    static constexpr std::int64_t ptr_low()
    {
      return impl::field_limits<PtrSize::value()>::low();
    }

    static constexpr std::int64_t diff_high()
    {
      return impl::field_limits<DiffSize::value()>::high();
    }

    static constexpr std::int64_t diff_low()
    {
      return impl::field_limits<DiffSize::value()>::low();
    }
  };

  template <typename SymbolIt>
  class agnostic_builder : public rlz::build::agnostic_observer<Alphabet, SymbolIt>
  {
  private:
    using Symbol = typename Alphabet::Symbol;
    enum State { FILLING, FILLED, FINISHED };

    using PK = parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>;
    using PL = PK::PointerLimits;

    PK pk_build;

    size_t source_len;

    std::int64_t block_ptr;

    std::vector<size_t> sub_blocks;
    sdsl::bit_vector phrase_indicator;

    State state;
    bool first_phrase;

  public:

    agnostic_builder() 
      : source_len(0U), block_ptr(PL::diff_low() - 1), state(FILLING), first_phrase(true)
    {
      pk_build.diffs.push_back(0U); // left-sentinel
    }

    std::tuple<std::size_t, std::size_t> can_split(
      std::size_t, std::ptrdiff_t, std::size_t copy_len, std::size_t junk_len
    ) override
    {
      assert(state == FILLING);
      return std::make_tuple(copy_len, junk_len);
    }

    bool split_as_block(
      std::size_t, std::ptrdiff_t copy_delta, std::size_t copy_len, std::size_t junk_len
    ) override
    {
      assert(state == FILLING);
      auto delta_delta  = copy_delta - block_ptr;
      return first_phrase or (
        (copy_len > 0) and (
          (delta_delta < PL::diff_low()) or (delta_delta > PL::diff_high())
        )
      );
    }

    void split(
      std::size_t phrase_start, std::ptrdiff_t copy_delta, std::size_t copy_len, std::size_t junk_len,
      const SymbolIt, bool block_split
    ) override
    {
      assert(state == FILLING);
      block_ptr         = block_split ? copy_delta : block_ptr;
      auto delta_delta  = copy_delta - block_ptr;

      // Set bit-vectors
      sub_blocks.push_back(phrase_start);
      if (phrase_indicator.size() < sub_blocks.size()) {
        phrase_indicator.resize(sub_blocks.size() * 2);
      }
      if (sub_blocks.size() > 1) {
        phrase_indicator[sub_blocks.size() - 2] = block_split;
      }

      // Saves ptr and delta
      if (block_split) {
        assert(delta_delta == 0);
        pk_build.ptrs.push_back(copy_delta);
      } else {
        assert(
          (copy_len == 0) or (
            (delta_delta >= PL::diff_low()) and (delta_delta <= PL::diff_high())
          )
        );
        pk_build.diffs.push_back(copy_len == 0 ? 0U : delta_delta);
      }

      source_len = phrase_start + copy_len + junk_len;
      first_phrase = false;      
    }

    // Parsing finished
    void finish() override
    {
      assert(state == FILLING);
      // Resize phrase indicator bitvector and corrects setting first sub-block as block begin
      phrase_indicator.resize(sub_blocks.size());
      phrase_indicator[sub_blocks.size() - 1] = 1; // Sentinel value

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

  template <typename SymbolIt>
  class builder : public  rlz::build::forward_observer_adapter<
                            parse_keeper<
                              Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep
                            >::agnostic_builder<SymbolIt>,
                            Alphabet,
                            SymbolIt
                          >,
                  public PointerLimits
  {
  private:
    using PK         = parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>;
    using ParentType = PK::agnostic_builder<SymbolIt>;

    ParentType wrapped;
  protected:
    ParentType *get_wrapped() override
    {
      return &wrapped;
    }
  public:
    PK get()
    {
      return wrapped.get();
    }
  };

  template <typename SymbolIt>
  class lcp_builder : public  rlz::lcp::build::lcp_observer_adapter<
                                parse_keeper<
                                  Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep
                                >::agnostic_builder<SymbolIt>,
                                Alphabet,
                                SymbolIt
                              >,
                      public PointerLimits
  {
  private:
    using PK         = parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>;
    using ParentType = PK::agnostic_builder<SymbolIt>;

    ParentType wrapped;
  protected:
    ParentType *get_wrapped() override
    {
      return &wrapped;
    }
  public:
    PK get()
    {
      return wrapped.get();
    }
  };
};

template <typename Alphabet, typename PtrSize, typename DiffSize, typename PhraseBVRep, typename SubphraseBVRep>
class parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>::iterator
  : public boost::iterator_facade<
        parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>::iterator,
        std::tuple<size_t, std::int64_t, size_t>,
        boost::forward_traversal_tag,
        std::tuple<size_t, std::int64_t, size_t>
    >
{
private:
  using Parent      = parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>;
  const Parent *ptr;

  struct Increment {
    size_t operator()(const size_t &i) const {
      return i + 1UL;
    }
  };

  // Ugly and not really generic (doesn't work when constness is not embedding into type), must find an another way to do it
  using PhraseIter = type_utils::RemoveQualifiers<decltype(ptr->pbv.begin())>;
  using RawSubIter = type_utils::RemoveQualifiers<decltype(ptr->sbv.get_bitset(0))>;
  using SubIter    = boost::transform_iterator<Increment, RawSubIter>;
  using PtrsIter   = typename parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>::PtrsIter;
  using DiffIter   = typename parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>::DiffIter;


  static constexpr std::array<std::uint64_t, 2> masks {{ 0xFFFFFFFFFFFFFFFF, 0x0000000000000000 }};

  PhraseIter  phrase_iter;
  SubIter     sub_iter;
  PtrsIter    ptr_iter;
  bool        is_absolute;
  DiffIter    diff_iter;
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
    phrase_ptr = *ptr_iter + (*diff_iter & masks[is_absolute]);
  }

public:
  iterator(const Parent *ptr, size_t phrase_index, size_t subphrase_index) // Normal iterator
    : ptr(ptr),
      phrase_iter(std::next(ptr->pbv.begin(), subphrase_index - (subphrase_index > 0))),
      // Messy index to compute length first phrase (one before, update in ctor body) and account special case sub_idx == 0
      sub_iter(boost::make_transform_iterator<Increment>(ptr->sbv.get_bitset((subphrase_index > 0) ? (subphrase_index - 1) : 0))),
      ptr_iter(std::next(ptr->ptrs.begin(), phrase_index)),
      is_absolute(subphrase_index > 0 and *phrase_iter),
      diff_iter(std::next(ptr->diffs.begin(), subphrase_index - phrase_index + is_absolute)),
      phrase_start(subphrase_index == 0 ? 0UL : *sub_iter),
      // So it's correct for subphrase_index == 0
      phrase_len(*sub_iter)
  {
    if (subphrase_index > 0) {
      ++phrase_iter;
    }
    if (subphrase_index > 0) {
      update_subphrase();
    }
    update_ptr();
  }

  iterator(const Parent *ptr) // End iterator
    : ptr(ptr),
      phrase_iter(ptr->pbv.end()),
      sub_iter(ptr->sbv.get_bitset_end()),
      ptr_iter(ptr->ptrs.end()),
      diff_iter(ptr->diffs.end())
  { }


private:
  friend class boost::iterator_core_access;

  bool equal(const iterator& other) const
  {
      return sub_iter == other.sub_iter;
  }

  void increment()
  {
    if (is_absolute == 0) {
      ++diff_iter;
    }
    is_absolute = *phrase_iter++;
    if (is_absolute) {
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

template <typename Alphabet, typename PtrSize, typename DiffSize, typename PhraseBVRep, typename SubphraseBVRep>
constexpr std::array<std::uint64_t, 2> parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>::iterator::masks;
}
