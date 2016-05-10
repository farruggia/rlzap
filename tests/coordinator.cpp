
#include <memory>

#include <alphabet.hpp>
#include <build_coordinator.hpp>

#include <gtest/gtest.h>
#include "coord_observer.hpp"

#include "main.hpp"

template <typename Alphabet, size_t max_delta = 8U>
class build_c1 : public rlz::build::observer<Alphabet> {
private:
  int block_delta;
  bool first_copy_phrase;
  using Symbol = typename Alphabet::Symbol;
public:

  build_c1() : block_delta(0), first_copy_phrase(true) { }

  // returns maximum prefix length which is OK to split.
  size_t can_split(size_t, size_t, size_t len, size_t junk_len, const Symbol *) override
  {
    return len + junk_len;
  }

  bool split_as_block(size_t source, size_t target, size_t len, size_t junk_len, const Symbol *bytes) override
  {
    int delta       = static_cast<int>(target) - static_cast<int>(source);
    if (first_copy_phrase && len > 0) {
      first_copy_phrase = false;
      return true;
    }
    return (len > 0) && std::abs(delta - block_delta) > max_delta;
  }

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  void split(size_t source, size_t target, size_t, size_t, const Symbol*, bool block) override
  {
    int delta       = static_cast<int>(target) - static_cast<int>(source);
    if (block) {
      block_delta = delta;
    }
  }

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet, size_t max_literal = 8U>
class build_c2 : public rlz::build::observer<Alphabet> {
public:
  using Symbol = typename Alphabet::Symbol;

  build_c2() { }
  // returns maximum prefix length which is OK to split.
  size_t can_split(size_t, size_t, size_t len, size_t junk_len, const Symbol *) override
  {
    return len + std::min(junk_len, max_literal);
  }

  bool split_as_block(size_t, size_t, size_t, size_t, const Symbol *) override { return false; } 

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  void split(size_t, size_t, size_t, size_t, const Symbol *, bool) override { } 

  // Parsing finished
  void finish() override { }
};

template <typename Alphabet, size_t max_copy_len = 10U>
class build_c3 : public rlz::build::observer<Alphabet> {
public:
  using Symbol = typename Alphabet::Symbol;

  build_c3() { }
  
  // returns maximum prefix length which is OK to split.
  size_t can_split(size_t, size_t, size_t len, size_t junk_len, const Symbol *) override
  {
    if (len > max_copy_len) {
      return max_copy_len;
    }
    return len + junk_len;
  }

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  bool split_as_block(size_t, size_t, size_t, size_t, const Symbol *) override
  {
    return false; 
  }

  void split(size_t, size_t, size_t, size_t, const Symbol *, bool) override
  {

  }

  // Parsing finished
  void finish() override
  {

  }
};

template <typename Alphabet>
class Coordinator : public ::testing::Test {
  std::shared_ptr<std::stringstream> ss;
public:
  using Symbol = typename Alphabet::Symbol;

  void SetUp() override { ss =  std::make_shared<std::stringstream>(); }

  rlz::build::coordinator<Alphabet> get()
  {
    std::vector<std::shared_ptr<rlz::build::observer<Alphabet>>> components {
      std::make_shared<build_c1<Alphabet>>(),
      std::make_shared<build_c2<Alphabet>>(),
      std::make_shared<build_c3<Alphabet>>(),
      std::make_shared<observer<Alphabet>>(ss)
    };
    std::vector<Symbol> buffer(300);
    return rlz::build::coordinator<Alphabet>(components.begin(), components.end(), buffer.data());
  }

  std::shared_ptr<std::stringstream> get_ss() { return ss; }
};

using Alphabets = ::testing::Types<
  rlz::alphabet::dna<>,
  rlz::alphabet::Integer<32UL>,
  rlz::alphabet::Integer<16UL>,
	rlz::alphabet::lcp_32
>;

TYPED_TEST_CASE(Coordinator, Alphabets);

TYPED_TEST(Coordinator, EndsWithCopy)
{
  auto ss = this->get_ss();
  auto c  = this->get();
  c.copy_evt(0, 0, 3);
  c.copy_evt(3, 3, 6);
  c.literal_evt(9, 20);
  c.copy_evt(29, 5, 6);
  c.copy_evt(35, 8, 6);
  c.copy_evt(41, 13, 3);
  c.copy_evt(44, 100, 20);
  c.end_evt();
  auto computed        = ss->str();
  std::string expected = 
    "{[C(0,0,3)][C(3,3,6)L(8)][L(8)][L(4)]}{[C(29,5,6)][C(35,8,6)][C(41,13,3)]}{[C(44,100,10)][C(54,110,10)]}.";
  ASSERT_EQ(expected, computed);
}

TYPED_TEST(Coordinator, EndsWithLiteral)
{
  auto ss = this->get_ss();
  auto c  = this->get();
  c.copy_evt(0, 0, 3);
  c.copy_evt(3, 3, 6);
  c.literal_evt(9, 20);
  c.copy_evt(29, 5, 6);
  c.copy_evt(35, 8, 6);
  c.copy_evt(41, 13, 3);
  c.copy_evt(44, 100, 20);
  c.literal_evt(64, 12);
  c.end_evt();
  auto computed        = ss->str();
  std::string expected = 
    "{[C(0,0,3)][C(3,3,6)L(8)][L(8)][L(4)]}{[C(29,5,6)][C(35,8,6)][C(41,13,3)]}{[C(44,100,10)][C(54,110,10)L(8)][L(4)]}.";
  ASSERT_EQ(expected, computed);
}
