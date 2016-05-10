#include <alphabet.hpp>
#include <parse_keeper.hpp>
#include <gtest/gtest.h>
#include "serialize.hpp"

#include <boost/range.hpp>

#include <build_coordinator.hpp>
#include <integer_type.hpp>

#include <memory>

#include "coord_observer.hpp"

#include "main.hpp"

template <typename Alphabet, std::int64_t ptr_low, std::int64_t ptr_high, std::int64_t diff_low, std::int64_t diff_high>
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
      ASSERT_GE(delta - last_delta, diff_low);
      ASSERT_LE(delta - last_delta, diff_high);
    }
  }

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet>
using ParseKeeper = rlz::parse_keeper<Alphabet, rlz::values::Size<16>,rlz::values::Size<4>>;

template <typename Alphabet>
using PkBuild = typename ParseKeeper<Alphabet>::builder;

template <typename Alphabet>
using CorrectObs = correctness_obs<
  Alphabet,
  PkBuild<Alphabet>::ptr_low,
  PkBuild<Alphabet>::ptr_high,
  PkBuild<Alphabet>::diff_low,
  PkBuild<Alphabet>::diff_high
>;

template <typename Alphabet>
class ParseKeep : public ::testing::Test {
public:
  using Symbol = typename Alphabet::Symbol;

  virtual void SetUp() { }

  ParseKeeper<Alphabet> get(std::vector<std::shared_ptr<rlz::build::observer<Alphabet>>> obs, const Symbol *buffer)
  {
    auto builder = std::make_shared<PkBuild<Alphabet>>();
    obs.push_back(builder);

    rlz::build::coordinator<Alphabet> c(obs.begin(), obs.end(), buffer);

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

TYPED_TEST_CASE(ParseKeep, Alphabets);

// (A) Build, No extra split
TYPED_TEST(ParseKeep, BuildNoExtraSplits)
{
  using namespace rlz;
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  auto ss       = std::make_shared<std::stringstream>();
  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<CorrectObs<Alphabet>>(),
    std::make_shared<observer<Alphabet>>(ss)
  }};

  std::vector<Symbol> buffer(150);

  this->get(observers, buffer.data());

  std::string exp = "{[C(0,0,30)L(20)][C(50,57,20)][C(70,62,20)L(10)]}{[C(100,90,10)][C(110,102,10)]}{[C(120,120,10)L(20)]}.";
  std::string get = ss->str();
  ASSERT_EQ(exp, get);
}

// (B) Build, copy split
template <typename Alphabet>
struct copy_obs : public rlz::build::observer<Alphabet> {
  using Symbol = typename Alphabet::Symbol;

  size_t can_split(size_t, size_t, size_t len, size_t junk, const Symbol *) override
  {
    return len > 15 ? 15 : len + junk;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t, size_t, size_t, size_t, const Symbol *) override { return false; }

  // Splits at given phrase.
  void split(size_t, size_t, size_t, size_t, const Symbol *, bool) override { }

  // Parsing finished
  void finish() override { }
};

TYPED_TEST(ParseKeep, BuildCopySplits)
{
  using namespace rlz;
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;

  auto ss       = std::make_shared<std::stringstream>();

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<CorrectObs<Alphabet>>(),
    std::make_shared<copy_obs<Alphabet>>(),
    std::make_shared<observer<Alphabet>>(ss)
  }};

  std::vector<Symbol> buffer(150);

  this->get(observers, buffer.data());

  std::string exp = "{[C(0,0,15)][C(15,15,15)L(20)][C(50,57,15)][C(65,72,5)][C(70,62,15)][C(85,77,5)L(10)]}{[C(100,90,10)][C(110,102,10)]}{[C(120,120,10)L(20)]}.";
  std::string get = ss->str();
  ASSERT_EQ(exp, get);  
}

// (C) Build, copy split + block split
template <typename Alphabet>
struct copy_block_obs : public rlz::build::observer<Alphabet> {
	using Symbol = typename Alphabet::Symbol;

  size_t can_split(size_t, size_t, size_t len, size_t junk, const Symbol *) override
  {
    return len > 15 ? 15 : len + junk;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t source, size_t, size_t, size_t, const Symbol *) override
  {
    return source == 15;
  }

  // Splits at given phrase.
  void split(size_t, size_t, size_t, size_t, const Symbol *, bool) override { }

  // Parsing finished
  void finish() override { }
};

TYPED_TEST(ParseKeep, BuildCopyBlock)
{
  using Alphabet = TypeParam;
  using namespace rlz;
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  auto ss       = std::make_shared<std::stringstream>();

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{
    std::make_shared<CorrectObs<Alphabet>>(),
    std::make_shared<copy_block_obs<Alphabet>>(),
    std::make_shared<observer<Alphabet>>(ss)
  }};

  std::vector<Symbol> buffer(150);

  this->get(observers, buffer.data());
  std::string exp = "{[C(0,0,15)]}{[C(15,15,15)L(20)][C(50,57,15)][C(65,72,5)][C(70,62,15)][C(85,77,5)L(10)]}{[C(100,90,10)][C(110,102,10)]}{[C(120,120,10)L(20)]}.";
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

TYPED_TEST(ParseKeep, PhraseSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->get(observers, buffer.data());

  check(pk, 0, 0, 0);
  check(pk, 1, 0, 0);
  check(pk, 30, 0, 0);
  check(pk, 49, 0, 0);
  check(pk, 50, 0, 1);
  check(pk, 69, 0, 1);
  check(pk, 70, 0, 2);
  check(pk, 89, 0, 2);
  check(pk, 90, 0, 2);
  check(pk, 99, 0, 2);
  check(pk, 100, 1, 3);
  check(pk, 109, 1, 3);
  check(pk, 110, 1, 4);
  check(pk, 119, 1, 4);
  check(pk, 120, 2, 5);
  check(pk, 130, 2, 5);
  check(pk, 149, 2, 5);
}

TYPED_TEST(ParseKeep, GetPtr)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->get(observers, buffer.data());

  ASSERT_EQ(0, pk.get_ptr(0, 0));
  ASSERT_EQ(7, pk.get_ptr(0, 1));
  ASSERT_EQ(-8, pk.get_ptr(0, 2));
  ASSERT_EQ(-10, pk.get_ptr(1, 3));
  ASSERT_EQ(-8, pk.get_ptr(1, 4));
  ASSERT_EQ(0, pk.get_ptr(2, 5));
}

TYPED_TEST(ParseKeep, StartSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->get(observers, buffer.data());

  ASSERT_EQ(0, pk.start_subphrase(0));
  ASSERT_EQ(50, pk.start_subphrase(1));
  ASSERT_EQ(70, pk.start_subphrase(2));
  ASSERT_EQ(100, pk.start_subphrase(3));
  ASSERT_EQ(110, pk.start_subphrase(4));
  ASSERT_EQ(120, pk.start_subphrase(5)); 
  ASSERT_EQ(150, pk.start_subphrase(6)); 
}

TYPED_TEST(ParseKeep, Length)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

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

TYPED_TEST(ParseKeep, FullIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->get(observers, buffer.data());

  // print_range(pk.get_iterator_begin(), pk.get_iterator_end());
  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0,   0,  50U),
    std::make_tuple<size_t, std::int64_t, size_t>(50,  7,  20U),
    std::make_tuple<size_t, std::int64_t, size_t>(70, -8,  30U),
    std::make_tuple<size_t, std::int64_t, size_t>(100,-10, 10U),
    std::make_tuple<size_t, std::int64_t, size_t>(110,-8,  10U),
    std::make_tuple<size_t, std::int64_t, size_t>(120, 0,  30U)
  }};

  auto parse_beg = pk.get_iterator_begin();
  auto parse_end = pk.get_iterator_end();
  auto exp_beg   = expected.begin();
  auto exp_end   = expected.end();

  ASSERT_EQ(expected.size(), std::distance(parse_beg, parse_end));
  ASSERT_EQ(boost::make_iterator_range(parse_beg, parse_end), boost::make_iterator_range(exp_beg, exp_end));
}

TYPED_TEST(ParseKeep, PartialIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0,   0,  50U),
    std::make_tuple<size_t, std::int64_t, size_t>(50,  7,  20U),
    std::make_tuple<size_t, std::int64_t, size_t>(70, -8,  30U),
    std::make_tuple<size_t, std::int64_t, size_t>(100,-10, 10U),
    std::make_tuple<size_t, std::int64_t, size_t>(110,-8,  10U),
    std::make_tuple<size_t, std::int64_t, size_t>(120, 0,  30U)
  }};

  std::vector<size_t> phrases {{ 0, 0, 0, 1, 1, 2 }};

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

TYPED_TEST(ParseKeep, SerialPhraseSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  check(pk, 0, 0, 0);
  check(pk, 1, 0, 0);
  check(pk, 30, 0, 0);
  check(pk, 49, 0, 0);
  check(pk, 50, 0, 1);
  check(pk, 69, 0, 1);
  check(pk, 70, 0, 2);
  check(pk, 89, 0, 2);
  check(pk, 90, 0, 2);
  check(pk, 99, 0, 2);
  check(pk, 100, 1, 3);
  check(pk, 109, 1, 3);
  check(pk, 110, 1, 4);
  check(pk, 119, 1, 4);
  check(pk, 120, 2, 5);
  check(pk, 130, 2, 5);
  check(pk, 149, 2, 5);
}

TYPED_TEST(ParseKeep, SerialGetPtr)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  ASSERT_EQ(0, pk.get_ptr(0, 0));
  ASSERT_EQ(7, pk.get_ptr(0, 1));
  ASSERT_EQ(-8, pk.get_ptr(0, 2));
  ASSERT_EQ(-10, pk.get_ptr(1, 3));
  ASSERT_EQ(-8, pk.get_ptr(1, 4));
  ASSERT_EQ(0, pk.get_ptr(2, 5));
}

TYPED_TEST(ParseKeep, SerialStartSubphrase)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  ASSERT_EQ(0, pk.start_subphrase(0));
  ASSERT_EQ(50, pk.start_subphrase(1));
  ASSERT_EQ(70, pk.start_subphrase(2));
  ASSERT_EQ(100, pk.start_subphrase(3));
  ASSERT_EQ(110, pk.start_subphrase(4));
  ASSERT_EQ(120, pk.start_subphrase(5)); 
  ASSERT_EQ(150, pk.start_subphrase(6)); 
}

TYPED_TEST(ParseKeep, SerialLength)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  ASSERT_EQ(pk.length(), buffer.size());

}

TYPED_TEST(ParseKeep, SerialFullIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  // print_range(pk.get_iterator_begin(), pk.get_iterator_end());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0,   0,  50U),
    std::make_tuple<size_t, std::int64_t, size_t>(50,  7,  20U),
    std::make_tuple<size_t, std::int64_t, size_t>(70, -8,  30U),
    std::make_tuple<size_t, std::int64_t, size_t>(100,-10, 10U),
    std::make_tuple<size_t, std::int64_t, size_t>(110,-8,  10U),
    std::make_tuple<size_t, std::int64_t, size_t>(120, 0,  30U)
  }};

  auto parse_beg = pk.get_iterator_begin();
  auto parse_end = pk.get_iterator_end();
  auto exp_beg   = expected.begin();
  auto exp_end   = expected.end();

  ASSERT_EQ(expected.size(), std::distance(parse_beg, parse_end));
  ASSERT_EQ(boost::make_iterator_range(parse_beg, parse_end), boost::make_iterator_range(exp_beg, exp_end));
}

TYPED_TEST(ParseKeep, SerialPartialIterator)
{
  using Alphabet = TypeParam;
  using Symbol   = typename Alphabet::Symbol;
  using namespace rlz;

  std::vector<std::shared_ptr<build::observer<Alphabet>>> observers;
  std::vector<Symbol> buffer(150);

  auto pk = this->unload_get(observers, buffer.data());

  std::vector<std::tuple<size_t, std::int64_t, size_t>> expected = {{
    std::make_tuple<size_t, std::int64_t, size_t>(0,   0,  50U),
    std::make_tuple<size_t, std::int64_t, size_t>(50,  7,  20U),
    std::make_tuple<size_t, std::int64_t, size_t>(70, -8,  30U),
    std::make_tuple<size_t, std::int64_t, size_t>(100,-10, 10U),
    std::make_tuple<size_t, std::int64_t, size_t>(110,-8,  10U),
    std::make_tuple<size_t, std::int64_t, size_t>(120, 0,  30U)
  }};

  std::vector<size_t> phrases {{ 0, 0, 0, 1, 1, 2 }};

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
