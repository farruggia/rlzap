#pragma once


#include "impl/api.hpp"

#include "coordinator.hpp"
#include "index.hpp"
#include "parse.hpp"

#include "../alphabet.hpp"
#include "../impl/differential_iterator.hpp"
#include "../dumper.hpp"
#include "../get_matchings.hpp"

#include <cassert>
#include <cstdint>
#include <type_traits>

namespace rlz { namespace lcp {

  template <
    typename ReferenceIt,
    typename LcpType         = std::uint32_t,
    std::size_t ExplicitBits = 32UL,
    std::size_t DeltaBits    =  4UL,
    std::size_t MaxLit       =  2UL,
    std::size_t SampleInt    = 64UL
  >
  using LcpIndex = typename detail::index_type<
                              ReferenceIt, LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt
                            >::type;

  template <
    typename LcpType         = std::uint32_t,
    std::size_t ExplicitBits = 32UL,
    std::size_t DeltaBits    =  4UL,
    std::size_t MaxLit       =  2UL,
    std::size_t SampleInt    = 64UL,
    typename InputIt,
    typename ReIt
  >
  LcpIndex<ReIt, LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt>
  index_build(
    InputIt input_begin, InputIt input_end,
    ReIt reference_begin, ReIt reference_end,
    std::size_t MaxLen = 1024UL,
    std::size_t LookAhead = 8UL, std::size_t ExplicitLen = 4UL
  )
  {
    static_assert(std::is_integral<LcpType>::value, "LcpType must be an integral type");
    static_assert(std::is_unsigned<LcpType>::value, "LcpType is signed, must be unsigned");
    static_assert(ExplicitBits < 64, "ExplicitBits must be less than 64 bits");
    assert((1UL << ExplicitBits) > (std::distance(reference_begin, reference_end) / 2));
    assert((1UL << ExplicitBits) > (std::distance(input_begin, input_end) / 2));
    using Index          = LcpIndex<ReIt, LcpType, ExplicitBits, DeltaBits, MaxLit, SampleInt>;
    using Alphabet       = typename detail::index_get_types<Index>::Alphabet;
    using Source         = typename detail::index_get_types<Index>::Source;
    using SignedAlphabet = rlz::alphabet::SignLcp<Alphabet>;
    using SignSymbol     = typename SignedAlphabet::Symbol;

    using DiffReIt = rlz::differential_iterator<ReIt, SignSymbol>;
    using ReDumper = rlz::utils::iterator_dumper<DiffReIt>;
    using DiffInIt = rlz::differential_iterator<InputIt, SignSymbol>;
    using InDumper = rlz::utils::iterator_dumper<DiffInIt>;

    ReDumper ref_dumper{
      DiffReIt(reference_begin), DiffReIt(reference_end)
    };
    InDumper input_dumper{
      DiffInIt(input_begin), DiffInIt(input_end)
    };

    auto matches = std::get<0>(
      rlz::get_relative_matches<SignedAlphabet>(ref_dumper, input_dumper)
    );

    using Builder = typename Index::template builder<InputIt>;
    using rlz::lcp::build::observer;
    using rlz::lcp::build::coordinator;

    auto build    = std::make_shared<Builder>();
    auto enforcer = std::make_shared<detail::phrase_limit<Alphabet, InputIt>>(MaxLen);

    std::vector<std::shared_ptr<observer<Alphabet, InputIt>>> v {{ build, enforcer }};
    coordinator<Alphabet, InputIt, ReIt> coord(
      v.begin(), v.end(), input_begin, reference_begin
    );

    auto lit_func = [&] (std::size_t pos, std::size_t length) {
      coord.literal_evt(pos, length);
    };

    auto copy_func = [&] (std::size_t pos, std::size_t ref, std::size_t len) {
      coord.copy_evt(pos, ref, len);
    };

    auto end_func = [&] () {
      coord.end_evt();
    };

    rlz::lcp::parser parse(DeltaBits, ExplicitBits, MaxLit, LookAhead, ExplicitLen);

    parse(
      matches.begin(), matches.end(),
      input_begin, input_end,
      reference_begin, reference_end,
      lit_func, copy_func, end_func
    );

    Source source{reference_begin, reference_end};
    return build->get(source);
  }
}}