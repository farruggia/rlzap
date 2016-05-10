#include <alphabet.hpp>
#include <build_coordinator.hpp>
#include <dna_packer.hpp>
#include <literal_keeper.hpp>
#include <trivial_prefix.hpp>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "coord_observer.hpp"
#include "serialize.hpp"

#include "main.hpp"

std::string input_string = "AACTTCAGGTGTCTTTGATGGAATCCTATTGGTNNNAANNTCAGGTAACGATTGAACTTCAATGGAATGAATTTTTTCTAAATTGAACCATTTANNATAACTTGGAATAACAATTTCATGTGCTTGCGGTATCTCAANNNNNNNNNNNNN";
std::string just_junk = "GGTNNNAANNTCAGGTAACGTTTANNATAAATCTCAANNNNNNNNNNNNN";

template <typename Alphabet>
struct get_strings {

  using Symbol = typename Alphabet::Symbol;

  std::vector<Symbol> get_input()
  {
    return std::vector<Symbol>(input_string.begin(), input_string.end());
  }

  std::vector<Symbol> get_junk()
  {
    return std::vector<Symbol>(just_junk.begin(), just_junk.end());
  }
};

template <typename Alphabet, std::int64_t max_literal_length>
struct correctness_obs : public rlz::build::observer<Alphabet> {

  using Symbol = typename Alphabet::Symbol;
  size_t prev_len;

  size_t can_split(size_t, size_t, size_t len, size_t junk, const Symbol *)
  {
    prev_len = len;
    return len + junk;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t, size_t, size_t, size_t, const Symbol *) { return false; }

  // Splits at given phrase.
  void split(size_t, size_t, size_t len, size_t junk, const Symbol *junk_ptr, bool)
  {
    ASSERT_EQ(len, prev_len);
    ASSERT_LE(junk, max_literal_length);

    // Debug
#if 0
    if (junk > 0) {
      std::cout << std::setw(2) << junk << " " 
                << std::string(junk_ptr, junk_ptr + junk)
                << std::endl;
    }
#endif
  }
  // Parsing finished
  void finish() { }
};

template <typename Alphabet>
struct get_type {
  using Type = rlz::literal_split_keeper<Alphabet, rlz::classic::prefix::trivial>;
};

template <typename Alphabet>
class ClassicLiteralKeeper : public ::testing::Test {
public:
  using Cumulative = rlz::classic::prefix::trivial;
  using Symbol     = typename Alphabet::Symbol;
  using Type       = typename get_type<Alphabet>::Type;
  static constexpr size_t max_literal_length = Type::max_literal_length;
  virtual void SetUp() { }

  static constexpr size_t max_len()
  {
    return max_literal_length;
  }

  Type get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet>>> obs, const Symbol *buffer)
  {
    using BuildType = typename rlz::literal_split_keeper<Alphabet, Cumulative>::builder;
    auto builder    = std::make_shared<BuildType>();
    obs.push_back(builder);
    rlz::build::coordinator<Alphabet> c(obs.begin(), obs.end(), buffer);

    c.copy_evt(0, 0, 30);       // New block: ptr = 0
    for (auto i = 30U; i < 50U; ++i) {
      c.literal_evt(i, 1);
    }
    c.copy_evt(50, 57, 40);     // delta = 7
    for (auto i = 90U; i < 90 + 10; ++i) {
      c.literal_evt(i, 1);  
    }
    c.copy_evt(100, 90, 30);    // New block: ptr = -10
    for (auto i = 130U; i < 130 + 20; ++i) {
      c.literal_evt(i, 1);  
    }
    c.end_evt();
    return builder->get();
  }

  std::string get_expected()
  {
    std::stringstream ss;
    ss << "[C(0,0,30)L(1)]";
    for (auto i = 1U; i < 20U; ++i) {
      ss << "[L(1)]";
    }
    ss << "[C(50,57,40)L(1)]";
    for (auto i = 1U; i < 10; ++i) {
      ss << "[L(1)]";
    }
    ss << "[C(100,90,30)L(1)]";
    for (auto i = 1U; i < 20U; ++i) {
      ss << "[L(1)]";
    }
    ss << "}.";
    return ss.str();
  }

  Type unload_get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet>>> obs, const Symbol *buffer)
  {
    return load_unload(get(obs, buffer));
  }

  std::vector<Symbol> get_input()
  {
    return get_strings<Alphabet>{}.get_input();
  }

  std::vector<Symbol> get_junk()
  {
    return get_strings<Alphabet>{}.get_junk();
  }  
};

using TPack = ::testing::Types<rlz::alphabet::dna<>, rlz::alphabet::Integer<16UL>, rlz::alphabet::lcp_32>;
TYPED_TEST_CASE(ClassicLiteralKeeper, TPack);

TYPED_TEST(ClassicLiteralKeeper, Build)
{
  using namespace rlz;
  auto ss    = std::make_shared<std::stringstream>();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
    std::make_shared<observer<Alphabet>>(ss)
  }};

  auto buffer = this->get_input();
  this->get(observers, buffer.data());

  std::string exp = this->get_expected();
  std::string get = ss->str();
  ASSERT_EQ(exp, get);  
}

TYPED_TEST(ClassicLiteralKeeper, Length)
{
  using namespace rlz;

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  
  auto buffer = this->get_input();
  
  auto lsk = this->get(observers, buffer.data());

  for (auto i = 0U; i < just_junk.size(); ++i) {
    ASSERT_EQ(lsk.literal_length(i), 1U);  
  }
}

template <typename It1, typename It2>
void range_equal(It1 exp_start, It2 got_start, size_t length)
{
  for (auto i = 0U; i < length; ++i) {
    auto exp = *exp_start++;
    auto got = *got_start++;
    ASSERT_EQ(exp, got);
  }
}

TYPED_TEST(ClassicLiteralKeeper, Access)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  auto lsk = this->get(observers, buffer.data());

  auto junks = this->get_junk();

  for (auto i = 0U; i < junks.size(); ++i) {
    auto sym = *lsk.literal_access(i);
    ASSERT_EQ(junks[i], sym);
  }
}

TYPED_TEST(ClassicLiteralKeeper, IterativeAccess)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  auto lsk = this->get(observers, buffer.data());

  // String composed only of literals
  auto literals = this->get_junk();

  for (auto i = 0U; i <= literals.size(); ++i) {
    for (auto j = i; j <= literals.size(); ++j) {
      auto lit_beg = std::next(lsk.literal_access(0), i);
      range_equal(lit_beg, std::next(literals.begin(), i), j - i);
    }
  }
}

TYPED_TEST(ClassicLiteralKeeper, LenIterator)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  auto lsk      = this->get(observers, buffer.data());
  auto literals = this->get_junk();

  for (auto i = 0U; i < literals.size(); ++i) {
    auto cmp_beg = lsk.get_iterator(i);
    auto cmp_end = lsk.get_iterator_end();
    for (auto it = cmp_beg; it != cmp_end; ++it) {
      ASSERT_EQ(1U, *it);
    }
  }
}

TYPED_TEST(ClassicLiteralKeeper, SerialLength)
{
  using namespace rlz;

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  
  auto buffer = this->get_input();
  auto lsk = this->unload_get(observers, buffer.data());
  
  for (auto i = 0U; i < just_junk.size(); ++i) {
    ASSERT_EQ(lsk.literal_length(i), 1U);  
  }
}

TYPED_TEST(ClassicLiteralKeeper, SerialAccess)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  auto lsk = this->unload_get(observers, buffer.data());

  auto junks = this->get_junk();

  for (auto i = 0U; i < junks.size(); ++i) {
    auto sym = *lsk.literal_access(i);
    ASSERT_EQ(junks[i], sym);
  }
}

TYPED_TEST(ClassicLiteralKeeper, SerialIterativeAccess)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  auto lsk = this->unload_get(observers, buffer.data());

  // String composed only of literals
  auto literals = this->get_junk();

  for (auto i = 0U; i <= literals.size(); ++i) {
    for (auto j = i; j <= literals.size(); ++j) {
      auto lit_beg = std::next(lsk.literal_access(0), i);
      range_equal(lit_beg, std::next(literals.begin(), i), j - i);
    }
  }
}

TYPED_TEST(ClassicLiteralKeeper, SerialLenIterator)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = TypeParam;
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<correctness_obs<Alphabet, max_literal_length>>(),
  }};
  auto lsk = this->unload_get(observers, buffer.data());

  auto literals = this->get_junk();

  for (auto i = 0U; i < literals.size(); ++i) {
    auto cmp_beg = lsk.get_iterator(i);
    auto cmp_end = lsk.get_iterator_end();
    for (auto it = cmp_beg; it != cmp_end; ++it) {
      ASSERT_EQ(1U, *it);
    }
  }
}
