#include <limits>
#include <memory>

#include <sstream>
#include <stdexcept>

#include <alphabet.hpp>
#include <get_matchings.hpp>
#include <lcp/coordinator.hpp>
#include <lcp/parse.hpp>

#include <gtest/gtest.h>

#include "include/parse_coord_common.hpp"
#include "main.hpp"

class build_sync {
public:
  build_sync() { reset(); }

  void set_len(std::size_t lit_len, std::size_t copy_len)
  {
    this->lit_len = std::min(this->lit_len, lit_len);
    this->copy_len = std::min(this->copy_len, copy_len);
  }

  void set_block() { is_block = true; } 

  std::size_t copy() { return copy_len; }
  std::size_t lit() { return lit_len; }
  bool block() { return is_block; }

  void reset()
  {
    copy_len = lit_len = std::numeric_limits<std::size_t>::max();
    is_block = false;
  }

private:
  std::size_t lit_len;
  std::size_t copy_len;
  bool is_block;
};

template <typename Alphabet, typename SymbolIt, size_t max_delta = 8U>
class build_c1 : public rlz::lcp::build::observer<Alphabet, SymbolIt>,
                 public std::enable_shared_from_this<build_c1<Alphabet, SymbolIt, max_delta>>
{
private:
  int block_delta;
  bool first_copy_phrase;
  build_sync &sync;
public:

  build_c1(build_sync &sync) : block_delta(0), first_copy_phrase(true), sync(sync) { }

  // returns maximum prefix length which is OK to split.
  size_t can_split(
    std::size_t, const SymbolIt, std::size_t junk_len, std::size_t, std::size_t len 
  ) override
  {
    sync.set_len(junk_len, len);
    return junk_len + len;
  }

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  bool split_as_block(
    std::size_t source,
    const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len
  ) override
  {
    source += junk_len;
    int delta       = static_cast<int>(target) - static_cast<int>(source);
    if (first_copy_phrase && len > 0) {
      first_copy_phrase = false;
      sync.set_block();
      return true;
    }
    auto to_ret = (len > 0) && std::abs(delta - block_delta) > max_delta;
    if (to_ret) {
      sync.set_block();
    }
    return to_ret;
  }

  void split(
    std::size_t source,
    const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len,
    bool block_split
  ) override
  {
    source += junk_len;
    int delta       = static_cast<int>(target) - static_cast<int>(source);
    if (block_split) {
      block_delta = delta;
    }
  }

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet, typename SymbolIt, size_t max_literal = 8U>
class build_c2 : public rlz::lcp::build::observer<Alphabet, SymbolIt>,
                 public std::enable_shared_from_this<build_c2<Alphabet, SymbolIt, max_literal>>

{
private:
  build_sync &sync;
public:
  using Symbol = typename Alphabet::Symbol;

  build_c2(build_sync &sync) : sync(sync) { }
  // returns maximum prefix length which is OK to split.
  size_t can_split(
    std::size_t, const SymbolIt, std::size_t junk_len, std::size_t, std::size_t len 
  ) override
  {
    if (junk_len <= max_literal) {
      sync.set_len(junk_len, len);
      return junk_len + len;
    }
    sync.set_len(max_literal, 0UL);
    return max_literal;
  }

  bool split_as_block(std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t) override
  {
    return false;
  } 

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  void split(std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t, bool) override { }

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet, typename SymbolIt, size_t max_copy_len = 10U>
class build_c3 : public rlz::lcp::build::observer<Alphabet, SymbolIt>,
                 public std::enable_shared_from_this<build_c3<Alphabet, SymbolIt, max_copy_len>>
{
private:
  build_sync &sync;
public:
  using Symbol = typename Alphabet::Symbol;

  build_c3(build_sync &sync) : sync(sync) { }
  
  // returns maximum prefix length which is OK to split.
  size_t can_split(
    std::size_t, const SymbolIt, std::size_t junk_len, std::size_t, std::size_t len 
  ) override
  {
    auto copy_len = std::min<std::size_t>(len, max_copy_len);
    sync.set_len(junk_len, copy_len);
    return junk_len + copy_len;
  }

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  bool split_as_block(std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t) override
  {
    return false; 
  }

  void split(std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t, bool) override { }

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet, typename SymbolIt, typename RefIt, size_t max_literal = 8U, size_t max_copy_len = 10U>
class coord_checker : public  rlz::lcp::build::observer<Alphabet, SymbolIt> {
private:
  bool prev_literal_only;
  std::size_t prev_lit_len;
  checker<SymbolIt, RefIt> C;
  build_sync &sync;
public:
  using Symbol = typename Alphabet::Symbol;

  coord_checker(SymbolIt src_begin, SymbolIt src_end, RefIt ref_begin, RefIt ref_end, build_sync &sync)
    : prev_literal_only(false), prev_lit_len(0UL),
      C(src_begin, src_end, ref_begin, ref_end),
      sync(sync)
  { }

  // returns maximum prefix length which is OK to split.
  size_t can_split(
    std::size_t, const SymbolIt, std::size_t junk_len, std::size_t, std::size_t len 
  ) override
  {
    return len + junk_len;
  }

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  bool split_as_block(std::size_t, const SymbolIt, std::size_t, std::size_t, std::size_t) override
  {
    return false; 
  }

  void split(
    std::size_t source,
    const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len,
    bool block_split
  ) override
  {
    // Check copy correctness
    if (junk_len > 0) {
      C.literal_evt(source, junk_len);
    }
    if (len > 0) {
      C.copy_evt(source + junk_len, target, len);
    }

    // We must check: (A) copy correctness (B) border correctness (C) limit correctness
    if (junk_len != sync.lit()) {
      std::stringstream ss;
      ss << source << ": Wrong literal length (Got: " << junk_len << ", Expected: " << sync.lit() << ")";
      throw std::logic_error(ss.str());
    }
    if (len != sync.copy()) {
      std::stringstream ss;
      ss << source << ": Wrong copy length (Got: " << len << ", Expected: " << sync.copy() << ")";
      throw std::logic_error(ss.str());      
    }
    if (block_split != sync.block()) {
      std::stringstream ss;
      ss  << source << ": Wrong border (Got: " 
          << std::boolalpha << block_split
          << ", Expected: " << std::boolalpha << sync.block() << ")";
      throw std::logic_error(ss.str());            
    }
    sync.reset();
  }

  // Parsing finished
  void finish() override { }
};

template <typename Length>
class LcpCoordinator : public ::testing::Test {
private:
  using Alphabet = rlz::alphabet::lcp_32;
  build_sync sync;
  constexpr const static size_t length = Length::value();
  using Symbol = typename Alphabet::Symbol;
  std::vector<Symbol> input, reference;
  refinput_get<length> getter;
public:

  void SetUp() override
  {
    // get input/reference
    auto ref_dna   = getter.reference();
    auto input_dna = getter.input();
    reference      = std::get<1>(dna_sa_lcp(ref_dna));
    input          = std::get<1>(dna_sa_lcp(input_dna));
  }

  std::tuple<
    Symbol*, Symbol*, // Input LCP
    Symbol*, Symbol*, // Reference LCP
    rlz::lcp::build::coordinator<Alphabet>
  > get()
  {
    using SymbolIt = typename Alphabet::Symbol*;
    std::vector<std::shared_ptr<rlz::lcp::build::observer<Alphabet, SymbolIt>>> components {
      std::make_shared<build_c1<Alphabet, SymbolIt>>(sync),
      std::make_shared<build_c2<Alphabet, SymbolIt>>(sync),
      std::make_shared<build_c3<Alphabet, SymbolIt>>(sync),
      std::make_shared<coord_checker<Alphabet, SymbolIt, SymbolIt>>(
        input.data(), input.data() + input.size(),
        reference.data(), reference.data() + reference.size(),
        sync
      )
    };

    return std::make_tuple(
      input.data(), input.data() + input.size(),
      reference.data(), reference.data() + reference.size(),
      rlz::lcp::build::coordinator<Alphabet, SymbolIt, SymbolIt>(
        components.begin(), components.end(), input.data(), reference.data()
      )
    );
  }
};

using Sizes = ::testing::Types<
  rlz::values::Size<4U>,
  rlz::values::Size<1024U>,
  rlz::values::Size<1048576U>,
  rlz::values::Size<2097152U>,
  rlz::values::Size<4194304U>
>;

TYPED_TEST_CASE(LcpCoordinator, Sizes);

template <typename Alphabet>
::testing::AssertionResult attempt_parse(
  rlz::lcp::build::coordinator<Alphabet> &coordinator,
  typename Alphabet::Symbol* input_begin, typename Alphabet::Symbol* input_end,
  typename Alphabet::Symbol* ref_begin, typename Alphabet::Symbol* ref_end
)
{
  auto inp_dumper = get_differential(input_begin, input_end);
  auto ref_dumper = get_differential(ref_begin, ref_end);

  auto matches = std::get<0>(
    rlz::get_relative_matches<rlz::alphabet::dlcp_32>(ref_dumper, inp_dumper)
  );

  rlz::lcp::parser parse;

  auto lit_func = [&] (std::size_t pos, std::size_t length) {
    coordinator.literal_evt(pos, length);
  };

  auto copy_func = [&] (std::size_t pos, std::size_t ref, std::size_t len) {
    coordinator.copy_evt(pos, ref, len);
  };

  auto end_func = [&] () {
    coordinator.end_evt();
  };

  try {
    parse(
      matches.begin(), matches.end(),
      input_begin, input_end,
      ref_begin, ref_end,
      lit_func, copy_func, end_func
    );
    return ::testing::AssertionSuccess();
  } catch (std::logic_error e) {    
    return ::testing::AssertionFailure() << e.what();
  }
}

TYPED_TEST(LcpCoordinator, Coordinate)
{
  std::uint32_t *input_beg, *input_end, *ref_beg, *ref_end;
  rlz::lcp::build::coordinator<rlz::alphabet::lcp_32> coordinator;
  std::tie(input_beg, input_end, ref_beg, ref_end, coordinator) = this->get();

  ASSERT_TRUE(attempt_parse(
      coordinator,
      input_beg, input_end,
      ref_beg, ref_end
  ));
}