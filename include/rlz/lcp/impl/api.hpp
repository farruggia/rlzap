#pragma once

#include "../../alphabet.hpp"
#include "../../containers.hpp"

#include "../index.hpp"

#include <cstdint>

namespace rlz { namespace lcp { namespace detail {

template <
  typename RefIt,
  typename LcpType,
  std::size_t ExplicitBits, std::size_t DeltaBits,
  std::size_t LiteralLen, std::size_t SampleInt
>
struct index_type {
  using Alphabet = rlz::alphabet::lcp<LcpType>;
  using Source   = rlz::iterator_container<Alphabet, RefIt>;
  using type =  rlz::lcp::index<
                  Alphabet,
                  Source,
                  rlz::parse_keeper<
                    Alphabet,
                    rlz::values::Size<ExplicitBits>,
                    rlz::values::Size<DeltaBits>,
                    rlz::vectors::dense, rlz::vectors::sparse
                  >,
                  rlz::literal_split_keeper<
                    Alphabet,
                    rlz::prefix::sampling_cumulative<
                      rlz::values::Size<LiteralLen>,
                      rlz::values::Size<SampleInt>
                    >
                  >
                >;
};

template <typename T>
struct index_get_types { };

template <typename AlphabetT, typename SourceT, typename PK, typename LK>
struct index_get_types<rlz::lcp::index<AlphabetT, SourceT, PK, LK>>
{
  using Alphabet = AlphabetT;
  using Source   = SourceT;
};


template <typename Alphabet, typename SymbolIt>
class phrase_limit: public rlz::lcp::build::observer<Alphabet, SymbolIt> {
  const std::size_t max_len;
public:
  phrase_limit(std::size_t max_len) : max_len(max_len) { }

  // returns maximum prefix length which is OK to split.
  std::size_t can_split(
    std::size_t, const SymbolIt, std::size_t junk_len, std::size_t, std::size_t len
  ) override
  {
    return std::min<std::size_t>(junk_len + len, max_len);
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(
    std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t
  ) override { return false; }

  // Splits at given phrase.
  void split(
    std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t, bool
  ) override { }

  // Parsing finished
  void finish() override { };
};


}}}