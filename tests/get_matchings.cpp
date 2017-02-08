#include <vector>

#include <alphabet.hpp>
#include <dumper.hpp>
#include <get_matchings.hpp>

#include "main.hpp"

using namespace rlz;

std::string reference_char = "AACTTCAGGTGTCTTTGATGGAATCCTATTGGTAAAAAATTCAGGTAACGATTGAACTTCAATGGAATGAATTTTTTCTAAATTGAACCATTTAGAATAACTTGGAATAACAATTTCATGTGCTTGCGGTATCTCAAAAGTTTCGGGTTC";
std::string input_char     = "AATCCTATTGGTAAAAAATTCAGGTAACAACTTCAGGTGTCTTTGATGTTCATGTGCTTGCGGTATCTCAAAAGTTTCGGGTTCAAAAAAAAAAAAAAAAAAAAAAAAAAAATTCTAAATTGAACCATTTAGAATAACTTGGCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCATTGGTAAAAAATTCAGGTAACGATT";

template <typename Alphabet>
struct get_refinput { };

template <typename Arg>
struct get_refinput<alphabet::dna<Arg>> {
  std::vector<char> reference()
  {
    return std::vector<char>(reference_char.begin(), reference_char.end());
  }

  std::vector<char> input()
  {
    return std::vector<char>(input_char.begin(), input_char.end());
  }

};

template <typename Arg>
struct get_refinput<alphabet::lcp<Arg>> {
  std::vector<Arg> reference()
  {
    return std::vector<Arg>(reference_char.begin(), reference_char.end());
  }

  std::vector<Arg> input()
  {
    return std::vector<Arg>(input_char.begin(), input_char.end());
  }

};

template <typename Alphabet>
class GetMatchings : public ::testing::Test {
  using Symbol = typename Alphabet::Symbol;
public:

  std::vector<Symbol> get_reference()
  {
    return get_refinput<Alphabet>{}.reference();
  }

  std::vector<Symbol> get_input()
  {
    return get_refinput<Alphabet>{}.input();
  }

  std::tuple<std::vector<match>, mapped_stream<Alphabet>, mapped_stream<Alphabet>> get()
  {
    std::stringstream input, reference;
    auto input_vec            = get_input();
    auto reference_vec        = get_reference();

    auto input_dump     = rlz::utils::get_iterator_dumper(input_vec.begin(), input_vec.end());
    auto reference_dump = rlz::utils::get_iterator_dumper(reference_vec.begin(), reference_vec.end());

    return get_relative_matches<Alphabet>(reference_dump, input_dump);
  }

};

using Alphabets = ::testing::Types<
  alphabet::dna<>,
  alphabet::lcp_32
>;

TYPED_TEST_CASE(GetMatchings, Alphabets);

template <typename T1, typename T2>
void check_iterables(const T1 &exp, const T2 &got)
{
  auto exp_len = std::distance(exp.begin(), exp.end());
  auto got_len = std::distance(got.begin(), got.end());
  ASSERT_EQ(exp_len, got_len);
  auto exp_it = exp.begin();
  auto got_it = got.begin();
  for (auto i = 0U; i < exp_len; ++i) {
    ASSERT_EQ(*exp_it++, *got_it++);
  }
}

TYPED_TEST(GetMatchings, Read)
{
  using Alphabet = TypeParam;
  std::vector<match> matches;
  mapped_stream<Alphabet> reference, input;
  std::tie(matches, reference, input) = this->get();
  auto exp_reference = this->get_reference();
  auto exp_input     = this->get_input();
  check_iterables(exp_reference, reference);
  check_iterables(exp_input, input);
}

TYPED_TEST(GetMatchings, Matches)
{
  using Alphabet = TypeParam;
  std::vector<match> matches;
  mapped_stream<Alphabet> reference, input;
  std::tie(matches, reference, input) = this->get();

  auto input_vec     = this->get_input();
  auto reference_vec = this->get_reference();
  // Step 1: check matches are valid
  ASSERT_EQ(input.size(), matches.size());
  for (auto i = 0U; i < matches.size(); ++i) {
    auto m = matches[i];
    auto i_start = i;
    auto i_end   = i + m.len;
    auto r_start = m.ptr;
    auto r_end   = m.ptr + m.len;
    ASSERT_LE(i_end, input.size());
    ASSERT_LE(r_end, reference.size());
    auto input_range     = boost::make_iterator_range(input.begin() + i_start, input.begin() + i_end);
    auto reference_range = boost::make_iterator_range(reference.begin() + r_start, reference.begin() + r_end);
    ASSERT_EQ(reference_range, input_range);
  }

  // Step 2: no longer matches
  for (auto i = 0U; i < matches.size(); ++i) {
    auto m = matches[i];
    auto i_start = i;
    auto i_end   = i + m.len + 1UL;
    if (i_end > input.size()) {
      continue; // Can't have a match longer than that
    }
    auto input_range     = boost::make_iterator_range(input.begin() + i_start, input.begin() + i_end);
    for (auto j = 0U; j < reference.size() - m.len - 1; ++j) {
      auto r_start = j;
      auto r_end   = j + m.len + 1UL;
      auto reference_range = boost::make_iterator_range(reference.begin() + r_start, reference.begin() + r_end);
      ASSERT_NE(reference_range, input_range);
    }
  }
}
