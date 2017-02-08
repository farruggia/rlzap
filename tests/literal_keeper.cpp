#include "main.hpp"
#include "serialize.hpp"

#include <alphabet.hpp>
#include <build_coordinator.hpp>
#include <dna_packer.hpp>
#include <literal_keeper.hpp>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <vector>

#include "coord_observer.hpp"

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

template <typename Alphabet, typename SymbolIt, std::int64_t max_literal_length>
struct correctness_obs : public rlz::build::observer<Alphabet, SymbolIt> {

  using Symbol = typename Alphabet::Symbol;
  size_t prev_len;

  size_t can_split(size_t, size_t, size_t len, size_t junk, const SymbolIt) override
  {
    prev_len = len;
    return len + junk;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t, size_t, size_t, size_t, const SymbolIt)  override { return false; }

  // Splits at given phrase.
  void split(size_t, size_t, size_t len, size_t junk, const SymbolIt junk_ptr, bool)  override
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
  void finish() override { }
};

template <typename TPack>
struct get_type {
  using Type = rlz::literal_split_keeper<typename TPack::Alphabet, typename TPack::Cumulative>;
};

template <typename TPack>
struct get_alphabet {
  using Type = typename TPack::Alphabet;
};

template <typename Types>
class LiteralKeeper : public ::testing::Test {
public:
  using Cumulative = typename Types::Cumulative;
  using Alphabet   = typename Types::Alphabet;
  using Symbol     = typename Alphabet::Symbol;
  using Type       = typename get_type<Types>::Type;
  static constexpr size_t max_literal_length = Type::max_literal_length;
  virtual void SetUp() { }

  static constexpr size_t max_len()
  {
    return max_literal_length;
  }

  Type get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet, const Symbol*>>> obs, const Symbol *buffer)
  {
    using BuildType = typename rlz::literal_split_keeper<Alphabet, Cumulative>::template builder<const Symbol*>;
    auto builder    = std::make_shared<BuildType>();
    obs.push_back(builder);
    rlz::build::coordinator<Alphabet, const Symbol*> c(obs.begin(), obs.end(), buffer);

    c.copy_evt(0, 0, 30);       // New block: ptr = 0
    c.literal_evt(30, 20);      
    c.copy_evt(50, 57, 20);     // delta = 7
    c.copy_evt(70, 62, 20);     // delta = -8
    c.literal_evt(90, 10);
    c.copy_evt(100, 90, 10);    // New block: ptr = -10
    c.copy_evt(110, 102, 10);   // delta = -8 [+2]
    c.copy_evt(120, 120, 10);   // New block: ptr = 0
    c.literal_evt(130, 20);
    c.end_evt();
    return builder->get();
  }

  Type unload_get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet, const typename Alphabet::Symbol*>>> obs, const Symbol *buffer)
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

template <typename Pack, typename Cum>
struct TypePack {
  using Alphabet     = Pack;
  using Cumulative = Cum;
};

using TPack = ::testing::Types<
  TypePack<rlz::alphabet::dna<>, rlz::prefix::fast_cumulative<rlz::vectors::sparse>>,
  TypePack<rlz::alphabet::dna<>, rlz::prefix::fast_cumulative<rlz::vectors::dense>>,
  TypePack<rlz::alphabet::dna<>, rlz::prefix::cumulative<rlz::vectors::sparse>>,
  TypePack<rlz::alphabet::dna<>, rlz::prefix::cumulative<rlz::vectors::dense>>,
  TypePack<rlz::alphabet::dna<>, rlz::prefix::sampling_cumulative<rlz::values::Size<4U>>>,
  TypePack<rlz::alphabet::Integer<16UL>, rlz::prefix::fast_cumulative<rlz::vectors::sparse>>,
  TypePack<rlz::alphabet::Integer<16UL>, rlz::prefix::fast_cumulative<rlz::vectors::dense>>,
  TypePack<rlz::alphabet::Integer<16UL>, rlz::prefix::cumulative<rlz::vectors::sparse>>,
  TypePack<rlz::alphabet::Integer<16UL>, rlz::prefix::cumulative<rlz::vectors::dense>>,
  TypePack<rlz::alphabet::Integer<16UL>, rlz::prefix::sampling_cumulative<rlz::values::Size<4U>>>,
  TypePack<rlz::alphabet::lcp_32, rlz::prefix::fast_cumulative<rlz::vectors::sparse>>,
  TypePack<rlz::alphabet::lcp_32, rlz::prefix::fast_cumulative<rlz::vectors::dense>>,
  TypePack<rlz::alphabet::lcp_32, rlz::prefix::cumulative<rlz::vectors::sparse>>,
  TypePack<rlz::alphabet::lcp_32, rlz::prefix::cumulative<rlz::vectors::dense>>,
  TypePack<rlz::alphabet::lcp_32, rlz::prefix::sampling_cumulative<rlz::values::Size<4U>>>
>;
TYPED_TEST_CASE(LiteralKeeper, TPack);

TYPED_TEST(LiteralKeeper, Build)
{
  using namespace rlz;
  auto ss    = std::make_shared<std::stringstream>();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;
  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
    std::make_shared<observer<Alphabet, const Symbol*>>(ss)
  }};

  auto buffer = this->get_input();
  this->get(observers, buffer.data());

  std::string exp = "[C(0,0,30)L(15)][L(5)][C(50,57,20)][C(70,62,20)L(10)][C(100,90,10)][C(110,102,10)][C(120,120,10)L(15)][L(5)]}.";
  std::string get = ss->str();
  ASSERT_EQ(exp, get);  
}

TYPED_TEST(LiteralKeeper, Length)
{
  using namespace rlz;

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;  
  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
  }};
  
  auto buffer = this->get_input();
  
  auto lsk = this->get(observers, buffer.data());
  
  ASSERT_EQ(lsk.literal_length(0), 15); // [C(0,0,30)L(15)]
  ASSERT_EQ(lsk.literal_length(1), 5);  // [L(5)]
  ASSERT_EQ(lsk.literal_length(2), 0);  // [C(50,57,20)]
  ASSERT_EQ(lsk.literal_length(3), 10); // [C(70,62,20)L(10)]
  ASSERT_EQ(lsk.literal_length(4), 0);  // [C(100,90,10)]
  ASSERT_EQ(lsk.literal_length(5), 0);  // [C(110,102,10)]
  ASSERT_EQ(lsk.literal_length(6), 15); // [C(120,120,10)L(15)]
  ASSERT_EQ(lsk.literal_length(7), 5);  // [L(5)]
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

TYPED_TEST(LiteralKeeper, Access)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;
  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
  }};
  auto lsk = this->get(observers, buffer.data());

  {
    // [C(0,0,30)L(15)]
    auto len        = lsk.literal_length(0);
    auto ptr_start  = lsk.literal_access(0);
    range_equal(ptr_start, std::next(buffer.begin(), 30), len);
  }
  {
    // [L(5)]
    auto len        = lsk.literal_length(1);
    auto ptr_start  = lsk.literal_access(1);
    range_equal(ptr_start, std::next(buffer.begin(), 45), len);
  }
  {
    // [C(50,57,20)]
    auto len        = lsk.literal_length(2);
    ASSERT_EQ(0, len);
  }
  {
    // [C(70,62,20)L(10)]
    auto len        = lsk.literal_length(3);
    auto ptr_start  = lsk.literal_access(3);
    range_equal(ptr_start, std::next(buffer.begin(), 90), len);
  }
  {
    // [C(100,90,10)]
    auto len        = lsk.literal_length(4);
    ASSERT_EQ(0, len);
  }
  {
    // [C(110,102,10)]
    auto len        = lsk.literal_length(5);
    ASSERT_EQ(0, len);
  }
  {
    // [C(120,120,10)L(15)]
    auto len        = lsk.literal_length(6);
    auto ptr_start  = lsk.literal_access(6);
    range_equal(ptr_start, std::next(buffer.begin(), 130), len);
  }
  {
    // [L(5)]
    auto len        = lsk.literal_length(7);
    auto ptr_start  = lsk.literal_access(7);
    range_equal(ptr_start, std::next(buffer.begin(), 145), len);
  }
}

TYPED_TEST(LiteralKeeper, IterativeAccess)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;

  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
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

TYPED_TEST(LiteralKeeper, LenIterator)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;

  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
  }};
  auto lsk = this->get(observers, buffer.data());

  std::vector<size_t> lengths = {15, 5, 0, 10, 0, 0, 15, 5};
  for (auto i = 0U; i < lengths.size(); ++i) {
    auto cmp_beg = lsk.get_iterator(i);
    auto cmp_end = lsk.get_iterator_end();
    auto exp_beg = std::next(lengths.begin(), i);
    auto exp_end = lengths.end();
    std::vector<size_t> cmp_values { cmp_beg, cmp_end };
    std::vector<size_t> exp_values { exp_beg, exp_end };
    ASSERT_EQ(cmp_values, exp_values);
  }
}

TYPED_TEST(LiteralKeeper, SerialLength)
{
  using namespace rlz;

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;

  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
  }};
  
  auto buffer = this->get_input();
  auto lsk = this->unload_get(observers, buffer.data());
  
  ASSERT_EQ(lsk.literal_length(0), 15); // [C(0,0,30)L(15)]
  ASSERT_EQ(lsk.literal_length(1), 5);  // [L(5)]
  ASSERT_EQ(lsk.literal_length(2), 0);  // [C(50,57,20)]
  ASSERT_EQ(lsk.literal_length(3), 10); // [C(70,62,20)L(10)]
  ASSERT_EQ(lsk.literal_length(4), 0);  // [C(100,90,10)]
  ASSERT_EQ(lsk.literal_length(5), 0);  // [C(110,102,10)]
  ASSERT_EQ(lsk.literal_length(6), 15); // [C(120,120,10)L(15)]
  ASSERT_EQ(lsk.literal_length(7), 5);  // [L(5)]
}

TYPED_TEST(LiteralKeeper, SerialAccess)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;
  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
  }};
  auto lsk = this->unload_get(observers, buffer.data());

  {
    // [C(0,0,30)L(15)]
    auto len        = lsk.literal_length(0);
    auto ptr_start  = lsk.literal_access(0);
    range_equal(ptr_start, std::next(buffer.begin(), 30), len);
  }
  {
    // [L(5)]
    auto len        = lsk.literal_length(1);
    auto ptr_start  = lsk.literal_access(1);
    range_equal(ptr_start, std::next(buffer.begin(), 45), len);
  }
  {
    // [C(50,57,20)]
    auto len        = lsk.literal_length(2);
    ASSERT_EQ(0, len);
  }
  {
    // [C(70,62,20)L(10)]
    auto len        = lsk.literal_length(3);
    auto ptr_start  = lsk.literal_access(3);
    range_equal(ptr_start, std::next(buffer.begin(), 90), len);
  }
  {
    // [C(100,90,10)]
    auto len        = lsk.literal_length(4);
    ASSERT_EQ(0, len);
  }
  {
    // [C(110,102,10)]
    auto len        = lsk.literal_length(5);
    ASSERT_EQ(0, len);
  }
  {
    // [C(120,120,10)L(15)]
    auto len        = lsk.literal_length(6);
    auto ptr_start  = lsk.literal_access(6);
    range_equal(ptr_start, std::next(buffer.begin(), 130), len);
  }
  {
    // [L(5)]
    auto len        = lsk.literal_length(7);
    auto ptr_start  = lsk.literal_access(7);
    range_equal(ptr_start, std::next(buffer.begin(), 145), len);
  }
}

TYPED_TEST(LiteralKeeper, SerialIterativeAccess)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;
  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
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

TYPED_TEST(LiteralKeeper, SerialLenIterator)
{
  using namespace rlz;

  auto buffer = this->get_input();

  using Type = typename get_type<TypeParam>::Type;
  constexpr auto max_literal_length = Type::max_literal_length;

  using Alphabet = typename get_alphabet<TypeParam>::Type;
  using Symbol = typename Alphabet::Symbol;
  std::vector<std::shared_ptr<build::observer<Alphabet, const Symbol*>>> observers {{
    std::make_shared<correctness_obs<Alphabet, const Symbol*, max_literal_length>>(),
  }};
  auto lsk = this->unload_get(observers, buffer.data());

  std::vector<size_t> lengths = {15, 5, 0, 10, 0, 0, 15, 5};
  for (auto i = 0U; i < lengths.size(); ++i) {
    auto cmp_beg = lsk.get_iterator(i);
    auto cmp_end = lsk.get_iterator_end();
    auto exp_beg = std::next(lengths.begin(), i);
    auto exp_end = lengths.end();
    std::vector<size_t> cmp_values { cmp_beg, cmp_end };
    std::vector<size_t> exp_values { exp_beg, exp_end };
    ASSERT_EQ(cmp_values, exp_values);
  }
}
