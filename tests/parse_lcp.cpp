#include <string>
#include <vector>

#include <dumper.hpp>
#include <get_matchings.hpp>
#include <match.hpp>
#include <lcp/parse.hpp>
#include <sa_compute.hpp>

#include <impl/differential_iterator.hpp>

#include "include/parse_coord_common.hpp"

#include <gtest/gtest.h>
#include "main.hpp"

template <typename Length>
class ParseLCP : public ::testing::Test {
  constexpr static std::size_t length = Length::value();
  std::string reference, input;
  std::vector<std::uint32_t> ref_lcp, inp_lcp;
public:

  const std::string &reference_get()
  {
    return reference;
  }

  const std::string &input_get()
  {
    return input;
  }

  std::vector<std::uint32_t> &reference_lcp()
  {
    return ref_lcp;
  }

  std::vector<std::uint32_t> &input_lcp()
  {
    return inp_lcp;
  }

  virtual void SetUp() {
    reference = refinput_get<length>{}.reference();
    ref_lcp   = std::get<1>(dna_sa_lcp(reference));
    input     = refinput_get<length>{}.input();
    inp_lcp   = std::get<1>(dna_sa_lcp(input));
  }
};

using Sizes = ::testing::Types<
  rlz::values::Size<4U>,
  rlz::values::Size<1024U>,
  rlz::values::Size<1048576U>,
  rlz::values::Size<2097152U>,
  rlz::values::Size<4194304U>
>;

TYPED_TEST_CASE(ParseLCP, Sizes);

template <
  typename MatchIt, typename InputIt, typename RefIt,
  typename LitFunc, typename CopyFunc, typename EndFunc
>
::testing::AssertionResult attempt_parse(
  MatchIt m_begin, MatchIt m_end,
  InputIt input_begin, InputIt input_end,
  RefIt ref_begin, RefIt ref_end,
  LitFunc lit_func,  CopyFunc copy_func, EndFunc end_func
) {
  rlz::lcp::parser parse(4, 32, 32);
  try {
    parse(
      m_begin, m_end,
      input_begin, input_end,
      ref_begin, ref_end,
      lit_func, copy_func, end_func
    );
    return ::testing::AssertionSuccess();
  } catch (std::logic_error e) {    
    return ::testing::AssertionFailure() << e.what();
  }
}

TYPED_TEST(ParseLCP, Parse)
{
  auto input     = this->input_get();
  auto reference = this->reference_get();
  auto input_lcp = this->input_lcp();
  auto ref_lcp   = this->reference_lcp();

  auto ref_dumper = get_differential(ref_lcp.begin(), ref_lcp.end());
  auto inp_dumper = get_differential(input_lcp.begin(), input_lcp.end());

  auto matches = std::get<0>(
    rlz::get_relative_matches<rlz::alphabet::dlcp_32>(ref_dumper, inp_dumper)
  );

  auto input_beg = input_lcp.data();
  auto input_end = input_lcp.data() + input_lcp.size();
  auto ref_beg   = ref_lcp.data();
  auto ref_end   = ref_lcp.data() + ref_lcp.size();

  checker<std::uint32_t*, std::uint32_t*> check(
    input_beg, input_end, ref_beg, ref_end
  );

  auto lit_func = [&] (std::size_t pos, std::size_t length) {
    check.literal_evt(pos, length);
  };

  auto copy_func = [&] (std::size_t pos, std::size_t ref, std::size_t len) {
    check.copy_evt(pos, ref, len);
  };

  auto end_func = [] () {};

  ASSERT_TRUE(attempt_parse(
      matches.begin(), matches.end(),
      input_beg, input_end,
      ref_beg, ref_end,
      lit_func, copy_func, end_func
  ));
}