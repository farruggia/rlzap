#include <alphabet.hpp>
#include <classic_parse_keeper.hpp>
#include <gtest/gtest.h>
#include "serialize.hpp"

#include <boost/range.hpp>

#include <build_coordinator.hpp>
#include <integer_type.hpp>

#include <memory>

#include "coord_observer.hpp"

#include "main.hpp"

template <typename Alphabet, std::int64_t ptr_low, std::int64_t ptr_high>
struct correctness_obs : public rlz::build::observer<Alphabet> {

  using Symbol = typename Alphabet::Symbol;

  std::int64_t last_delta;

  size_t can_split(size_t, size_t, size_t len, size_t junk, const Symbol *) override
  {
    return len + junk;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t, size_t, size_t, size_t, const Symbol *) override
  { 
    return false;
  }

  // Splits at given phrase.
  void split(size_t source, size_t target, size_t, size_t, const Symbol *, bool is_block) override
  {
    auto delta = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(source);
    if (is_block) {
      ASSERT_GE(delta, ptr_low);
      ASSERT_LE(delta, ptr_high);
      last_delta = delta;
    } else {
      ASSERT_EQ(delta, last_delta);
    }
  }

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet>
using ParseKeeper = rlz::classic::parse_keeper<Alphabet, rlz::values::Size<16>>;

template <typename Alphabet>
using PkBuild = typename ParseKeeper<Alphabet>::builder;

template <typename Alphabet>
using CorrectObs = correctness_obs<
  Alphabet,
  PkBuild<Alphabet>::ptr_low,
  PkBuild<Alphabet>::ptr_high
>;

template <typename Alphabet>
class ClassicParseKeep : public ::testing::Test {
public:
  using Symbol = typename Alphabet::Symbol;

  virtual void SetUp() { }

  ParseKeeper<Alphabet> get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet>>> obs, const Symbol *buffer)
  {
    auto builder = std::make_shared<PkBuild<Alphabet>>();
    obs.push_back(builder);

    rlz::build::coordinator<Alphabet> c(obs.begin(), obs.end(), buffer);

    c.literal_evt(0, 1);    // New block, ptr = 0
    c.literal_evt(1, 1);
    c.literal_evt(2, 1);
    c.copy_evt(3, 3, 26);   // Same block, still ptr = 0
    c.literal_evt(29, 1);
    c.copy_evt(30, 40, 29); // New block, ptr = 10
    c.literal_evt(59, 1);
    c.copy_evt(60, 70, 19); // Same block, ptr = 10
    c.literal_evt(79, 1);
    c.copy_evt(80, 80, 19); // New block, ptr = 0
    c.literal_evt(99, 1);
    c.end_evt();

    return builder->get();
  }

  ParseKeeper<Alphabet> unload_get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet>>> obs, const Symbol *buffer)
  {
    return load_unload(get(obs, buffer));
  }
};

using Alphabets = ::testing::Types<
  rlz::alphabet::dna<>,
  rlz::alphabet::Integer<32UL>,
  rlz::alphabet::Integer<16UL>,
  rlz::alphabet::lcp_32
>;

TYPED_TEST_CASE(ClassicParseKeep, Alphabets);

TYPED_TEST(ClassicParseKeep, Build)
{
  using namespace rlz;
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  auto ss       = std::make_shared<std::stringstream>();
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<CorrectObs<Alphabet>>(),
    std::make_shared<observer<Alphabet>>(ss)
  }};

  std::vector<Symbol> buffer(100);

  this->get(observers, buffer.data());

  std::string exp = "{[L(1)][L(1)][L(1)][C(3,3,26)L(1)]}{[C(30,40,29)L(1)][C(60,70,19)L(1)]}{[C(80,80,19)L(1)]}.";
  std::string get = ss->str();
  ASSERT_EQ(exp, get);
}

template <typename PK>
void check(PK &pk, size_t position, size_t phrase, size_t subphrase)
{
  size_t r_p, r_s;
  std::tie(r_p, r_s) = pk.phrase_subphrase(position);
  ASSERT_EQ(phrase, r_p);
  ASSERT_EQ(subphrase, r_s);
}

TYPED_TEST(ClassicParseKeep, PhraseSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->get(observers, buffer.data());

  check(pk, 0, 0, 0);
  check(pk, 1, 0, 1);
  check(pk, 2, 0, 2);
  check(pk, 3, 0, 3);
  check(pk, 28, 0, 3);
  check(pk, 29, 0, 3);
  check(pk, 30, 1, 4);
  check(pk, 40, 1, 4);
  check(pk, 58, 1, 4);
  check(pk, 59, 1, 4);
  check(pk, 60, 1, 5);
  check(pk, 79, 1, 5);
  check(pk, 80, 2, 6);
  check(pk, 99, 2, 6);
}

TYPED_TEST(ClassicParseKeep, GetPtr)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->get(observers, buffer.data());

  ASSERT_EQ(0, pk.get_ptr(0, 0));
  ASSERT_EQ(0, pk.get_ptr(0, 1));
  ASSERT_EQ(0, pk.get_ptr(0, 2));
  ASSERT_EQ(0, pk.get_ptr(0, 3));
  ASSERT_EQ(10, pk.get_ptr(1, 4));
  ASSERT_EQ(10, pk.get_ptr(1, 5));
  ASSERT_EQ(0, pk.get_ptr(2, 6));
}

TYPED_TEST(ClassicParseKeep, StartSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->get(observers, buffer.data());

  ASSERT_EQ(0, pk.start_subphrase(0));
  ASSERT_EQ(1, pk.start_subphrase(1));
  ASSERT_EQ(2, pk.start_subphrase(2));
  ASSERT_EQ(3, pk.start_subphrase(3));
  ASSERT_EQ(30, pk.start_subphrase(4));
  ASSERT_EQ(60, pk.start_subphrase(5)); 
  ASSERT_EQ(80, pk.start_subphrase(6)); 
}

TYPED_TEST(ClassicParseKeep, Length)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->get(observers, buffer.data());

  ASSERT_EQ(pk.length(), buffer.size());

}

template <typename Os>
Os &operator<<(Os &s, const std::tuple<std::int64_t, size_t> &t)
{
  std::int64_t ptr;
  size_t len;
  std::tie(ptr, len) = t;
  s << "<" << ptr << ", " << len << ">";
  return s;
}

template <typename It>
void print_range(It begin, It end)
{
  for (auto i : boost::make_iterator_range(begin, end)) {
    std::cout << i << " ";
  }
  std::cout << std::endl;
}

TYPED_TEST(ClassicParseKeep, FullIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->get(observers, buffer.data());

  // print_range(pk.get_iterator_begin(), pk.get_iterator_end());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(1U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(2U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(3U,   0, 27),
    std::make_tuple<size_t, std::int64_t, size_t>(30U, 10, 30),
    std::make_tuple<size_t, std::int64_t, size_t>(60U, 10, 20),
    std::make_tuple<size_t, std::int64_t, size_t>(80U, 0,  20)
  }};

  auto parse_beg = pk.get_iterator_begin();
  auto parse_end = pk.get_iterator_end();
  auto exp_beg   = expected.begin();
  auto exp_end   = expected.end();

  ASSERT_EQ(expected.size(), std::distance(parse_beg, parse_end));
  ASSERT_EQ(boost::make_iterator_range(parse_beg, parse_end), boost::make_iterator_range(exp_beg, exp_end));
}

TYPED_TEST(ClassicParseKeep, PartialIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(1U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(2U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(3U,   0, 27),
    std::make_tuple<size_t, std::int64_t, size_t>(30U, 10, 30),
    std::make_tuple<size_t, std::int64_t, size_t>(60U, 10, 20),
    std::make_tuple<size_t, std::int64_t, size_t>(80U, 0,  20)
  }};

  std::vector<size_t> phrases {{ 0, 0, 0, 0, 1, 1, 2 }};

  for (auto i = 0U; i < expected.size(); ++i) {
    for (auto j = i; j < expected.size(); ++j) {
      auto parse_beg = pk.get_iterator(phrases[i], i);
      auto parse_end = pk.get_iterator(phrases[j], j);      
      auto exp_beg   = std::next(expected.begin(), i);
      auto exp_end = std::next(expected.begin(), j);
      ASSERT_EQ(j - i, std::distance(parse_beg, parse_end));
      ASSERT_EQ(boost::make_iterator_range(parse_beg, parse_end), boost::make_iterator_range(exp_beg, exp_end));
    }
  }
}

TYPED_TEST(ClassicParseKeep, SerialPhraseSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  check(pk, 0, 0, 0);
  check(pk, 1, 0, 1);
  check(pk, 2, 0, 2);
  check(pk, 3, 0, 3);
  check(pk, 28, 0, 3);
  check(pk, 29, 0, 3);
  check(pk, 30, 1, 4);
  check(pk, 40, 1, 4);
  check(pk, 58, 1, 4);
  check(pk, 59, 1, 4);
  check(pk, 60, 1, 5);
  check(pk, 79, 1, 5);
  check(pk, 80, 2, 6);
  check(pk, 99, 2, 6);
}

TYPED_TEST(ClassicParseKeep, SerialGetPtr)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  ASSERT_EQ(0, pk.get_ptr(0, 0));
  ASSERT_EQ(0, pk.get_ptr(0, 1));
  ASSERT_EQ(0, pk.get_ptr(0, 2));
  ASSERT_EQ(0, pk.get_ptr(0, 3));
  ASSERT_EQ(10, pk.get_ptr(1, 4));
  ASSERT_EQ(10, pk.get_ptr(1, 5));
  ASSERT_EQ(0, pk.get_ptr(2, 6));
}

TYPED_TEST(ClassicParseKeep, SerialStartSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  ASSERT_EQ(0, pk.start_subphrase(0));
  ASSERT_EQ(1, pk.start_subphrase(1));
  ASSERT_EQ(2, pk.start_subphrase(2));
  ASSERT_EQ(3, pk.start_subphrase(3));
  ASSERT_EQ(30, pk.start_subphrase(4));
  ASSERT_EQ(60, pk.start_subphrase(5)); 
  ASSERT_EQ(80, pk.start_subphrase(6)); 
}

TYPED_TEST(ClassicParseKeep, SerialLength)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  ASSERT_EQ(pk.length(), buffer.size());

}

TYPED_TEST(ClassicParseKeep, SerialFullIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  // print_range(pk.get_iterator_begin(), pk.get_iterator_end());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(1U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(2U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(3U,   0, 27),
    std::make_tuple<size_t, std::int64_t, size_t>(30U, 10, 30),
    std::make_tuple<size_t, std::int64_t, size_t>(60U, 10, 20),
    std::make_tuple<size_t, std::int64_t, size_t>(80U, 0,  20)
  }};

  auto parse_beg = pk.get_iterator_begin();
  auto parse_end = pk.get_iterator_end();
  auto exp_beg   = expected.begin();
  auto exp_end   = expected.end();

  ASSERT_EQ(expected.size(), std::distance(parse_beg, parse_end));
  ASSERT_EQ(boost::make_iterator_range(parse_beg, parse_end), boost::make_iterator_range(exp_beg, exp_end));
}

TYPED_TEST(ClassicParseKeep, SerialPartialIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(100);

  auto pk = this->unload_get(observers, buffer.data());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(1U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(2U,   0, 1U),
    std::make_tuple<size_t, std::int64_t, size_t>(3U,   0, 27),
    std::make_tuple<size_t, std::int64_t, size_t>(30U, 10, 30),
    std::make_tuple<size_t, std::int64_t, size_t>(60U, 10, 20),
    std::make_tuple<size_t, std::int64_t, size_t>(80U, 0,  20)
  }};

  std::vector<size_t> phrases {{ 0, 0, 0, 0, 1, 1, 2 }};

  for (auto i = 0U; i < expected.size(); ++i) {
    for (auto j = i; j < expected.size(); ++j) {
      auto parse_beg = pk.get_iterator(phrases[i], i);
      auto parse_end = pk.get_iterator(phrases[j], j);      
      auto exp_beg   = std::next(expected.begin(), i);
      auto exp_end = std::next(expected.begin(), j);
      ASSERT_EQ(j - i, std::distance(parse_beg, parse_end));
      ASSERT_EQ(boost::make_iterator_range(parse_beg, parse_end), boost::make_iterator_range(exp_beg, exp_end));
    }
  }
}
