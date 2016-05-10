#include <index.hpp>

#include <alphabet.hpp>
#include <build_coordinator.hpp>
#include <integer_type.hpp>
#include <prefix_sum.hpp>
#include <impl/type_unpack.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>

#include <boost/range/iterator_range_core.hpp> 

#include "serialize.hpp"
#include "main.hpp"

template <typename Alphabet>
class string_adapt {
public:
  using Symbol = typename Alphabet::Symbol;
private:
  const Symbol *begin_;
  const Symbol *end_;
public:
  string_adapt() : string_adapt(nullptr, nullptr) { }
  string_adapt(const Symbol *begin_, const Symbol *end_) : begin_(begin_), end_(end_) { }
  string_adapt(const Symbol *begin_, size_t len) : begin_(begin_), end_(begin_ + len) { }
  string_adapt(const std::vector<Symbol> &vec) : string_adapt(vec.data(), vec.size()) { }

  const Symbol *begin() const { return begin_; }
  const Symbol *end() const { return end_; }
  const size_t size() const { return std::distance(begin(), end()); }
  Symbol operator[](size_t idx) const { assert(idx < (end_ - begin_)); return begin_[idx]; }
};


namespace rlz { namespace test {

std::string reference = "AACTTCAGGTGTCTTTGATGGAATCCTATTGNNNANNAATTCAGGTAACGATTGAACTTCAATGGAATGAATTTTTTCTAAATTGAACCANNNNNNNNNNCTTGGAATAACAATTTCATGTGCTTGCGGTATCTCAAAAGTTTCGGGTNN";

template <typename Alphabet>
struct get_reference {
	using Symbol = typename Alphabet::Symbol;

  std::vector<Symbol> operator()()
  {
    std::vector<Symbol> to_ret(reference.size());
    std::copy(reference.begin(), reference.end(), to_ret.begin());
    return to_ret;
  }
};

template <typename TypeList>
struct unpack
{ };

template <typename AlphabetT, typename ReferenceT, typename ParseKeeperT, typename LiteralSplitKeeperT>
struct unpack<impl::type_list<AlphabetT, ReferenceT, ParseKeeperT, LiteralSplitKeeperT>>
{
  using  Alphabet           = AlphabetT;
  using  Reference          = ReferenceT;
  using  ParseKeeper        = ParseKeeperT;
  using  LiteralSplitKeeper = LiteralSplitKeeperT;
};

template <typename TypeList>
class Index : public ::testing::Test {
public:
  using IndexType   = impl::Compone<index, TypeList>;
  using Alphabet    = typename IndexType::AlphabetType;
  using Symbol      = typename Alphabet::Symbol;
  using SourceType  = typename IndexType::ReferenceType;
private:
  void reference_add(size_t begin, size_t len)
  {
    auto reference = reference_get();
    source.insert(
      source.end(),
      std::next(reference.begin(), begin),
      std::next(reference.begin(), begin + len)
    );
  }
public:
  IndexType index;
  std::vector<Symbol> source;
  std::vector<Symbol> ref_sym;

  SourceType reference_wrap()
  {
    return SourceType(ref_sym);
  }

  std::vector<Symbol> reference_get()
  {
    return get_reference<Alphabet>{}();
  }

  std::vector<Symbol> source_get()
  {
    return source;
  }

  virtual void SetUp() override
  {
    ref_sym = reference_get();
    auto build = std::make_shared<typename IndexType::builder>();
    std::vector<std::shared_ptr<build::observer<Alphabet>>> observers {{ build }};
    
    // Step 1: build source from reference
    reference_add(0, 30);      // Copy
    reference_add(0, 20);      // Literal
    reference_add(57, 20);     // Copy
    reference_add(62, 20);     // Copy
    reference_add(0, 10);      // Literal
    reference_add(90, 10);     // Copy
    reference_add(102, 10);    // Copy
    reference_add(120, 10);    // Copy
    reference_add(0, 20);      // Literal
    assert(source.size() == 150);

    // Step 2: build the index
    build::coordinator<Alphabet> c(observers.begin(), observers.end(), source.data());
    c.copy_evt(0, 0, 30);       
    c.literal_evt(30, 20);      // [0] 0,1 block (-> 45, -> 50)
    c.copy_evt(50, 57, 20);     //     2   block (->70)
    c.copy_evt(70, 62, 20);     
    c.literal_evt(90, 10);      //     3   block (-> 100)
    c.copy_evt(100, 90, 10);    // [1] 4   block (-> 110)
    c.copy_evt(110, 102, 10);   //     5   block (-> 120)
    c.copy_evt(120, 120, 10);
    c.literal_evt(130, 20);     // [2] 6,7 block (-> 130, -> 145)
    c.end_evt();
    index = build->get(reference_wrap());
  }

  IndexType unload()
  {
    auto to_ret = load_unload(index);
    to_ret.set_source(reference_wrap());
    return to_ret;
  }
};

using IndexTypes = ::testing::Types<

  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>, 
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::fast_cumulative<vectors::sparse>>
  >,
  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>,
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::fast_cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>,
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::fast_cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>, 
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::cumulative<vectors::sparse>>
  >,
  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>,
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>,
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::lcp_32,
    string_adapt<rlz::alphabet::lcp_32>,
    parse_keeper<rlz::alphabet::lcp_32, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::lcp_32, rlz::prefix::sampling_cumulative<>>
  >,

  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>, 
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::fast_cumulative<vectors::sparse>>
  >,
  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>,
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::fast_cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>,
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::fast_cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>, 
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::cumulative<vectors::sparse>>
  >,
  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>,
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>,
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::Integer<16UL>,
    string_adapt<rlz::alphabet::Integer<16UL>>,
    parse_keeper<rlz::alphabet::Integer<16UL>, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::Integer<16UL>, rlz::prefix::sampling_cumulative<>>
  >,

  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>, 
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::fast_cumulative<vectors::sparse>>
  >,
  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>,
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::fast_cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>,
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::fast_cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>, 
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::cumulative<vectors::sparse>>
  >,
  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>,
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::dense, vectors::sparse>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>,
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::cumulative<vectors::dense>>
  >,
  impl::type_list<
    rlz::alphabet::dna<>,
    string_adapt<rlz::alphabet::dna<>>,
    parse_keeper<rlz::alphabet::dna<>, values::Size<16>, values::Size<4>, vectors::sparse, vectors::dense>,
    literal_split_keeper<rlz::alphabet::dna<>, rlz::prefix::sampling_cumulative<>>
  >

>;
TYPED_TEST_CASE(Index, IndexTypes);

TYPED_TEST(Index, Size)
{
  ASSERT_EQ(this->source_get().size(), this->index.size());
}

template <typename It1, typename It2>
::testing::AssertionResult check_eq(It1 exp_begin, It1 exp_end, It2 got_begin, It2 got_end)
{
  auto len_1 = std::distance(exp_begin, exp_end);
  auto len_2 = std::distance(got_begin, got_end);
  if (len_1 != len_2) {
    return ::testing::AssertionFailure() << "Length of first sequence "
                                         << "(" << len_1 << ") "
                                         << "is different from length of second sequence"
                                         << "(" << len_2 << ") ";
  }

  auto exp_range = boost::make_iterator_range(exp_begin, exp_end);
  auto got_range = boost::make_iterator_range(got_begin, got_end);
  if (exp_range != got_range) {
    std::stringstream ss_exp, ss_got;
    for (auto i : exp_range) {
      ss_exp << i << " ";
    }
    for (auto i : got_range) {
      ss_got << i << " ";
    }

    return ::testing::AssertionFailure() << "Ranges are different: \n"
                                         << "EXP = " << ss_exp.str() << "\n"
                                         << "GOT = " << ss_got.str() << "\n";

  }
  return ::testing::AssertionSuccess();
}

template <typename Symbol>
::testing::AssertionResult check_eq(const std::vector<Symbol> &exp, const std::vector<Symbol> &got)
{
  return check_eq(exp.begin(), exp.end(), got.begin(), got.end());
}

TYPED_TEST(Index, CharacterAccess)
{
  using Alphabet = typename unpack<TypeParam>::Alphabet;
  using Symbol   = typename Alphabet::Symbol;

  auto source = this->source_get();
  std::vector<Symbol> got;
  for (auto i = 0U; i < source.size(); ++i) {
    got.push_back(this->index(i));
  }
  ASSERT_TRUE(check_eq(source, got));
}

TYPED_TEST(Index, RangeAccess)
{
  using Alphabet = typename unpack<TypeParam>::Alphabet;
  using Symbol   = typename Alphabet::Symbol;
  auto source    = this->source_get();
  std::vector<Symbol> storage(source.size());
  for (auto start = 0U; start < source.size(); ++start) {
    for (auto end = start + 1; end <= source.size(); ++end) {
      this->index(start, end, storage.data());
      ASSERT_TRUE(check_eq(
        std::next(source.begin(), start),
        std::next(source.begin(), end),
        storage.begin(),
        std::next(storage.begin(), end - start)
      ));
      if (this->HasFatalFailure()) {
        std::cout << "Failure on range " << start << ", " << end << std::endl;
        return;
      }
    }
  }
}

TYPED_TEST(Index, SerialSize)
{
  auto idx = this->unload();
  auto source = this->source_get();
  ASSERT_EQ(source.size(), idx.size());
}

using ds::impl::sign;
using ds::impl::unsign;

std::int64_t get_diff(size_t source, size_t target)
{
  return static_cast<std::int64_t>(target) - static_cast<std::int64_t>(source);
}

template <size_t Width>
size_t perform_conversions(size_t source, size_t target, unsign<Width> u, sign<Width> s, target_get<Width> &w)
{
  auto ptr        = get_diff(source, target);
  auto serialized = u(ptr);
  ptr             = s(serialized);
  target          = w(source, ptr);
  // target          = source + ptr;
  return target;
}

TYPED_TEST(Index, target_get)
{
  using ParseKeeper = typename unpack<TypeParam>::ParseKeeper;
  static const constexpr size_t ptr_size = ParseKeeper::ptr_size;

  // Pointers in (unsigned) 32-bits integer serialized. 
  // Original pointers in signed 64-bits ptr.
  unsign<ptr_size> usgn;
  sign<ptr_size> sgn;

  size_t len    = (1UL << (ptr_size - 2)) * 3;
  target_get<ptr_size> get_target(len);

  ASSERT_EQ(100, perform_conversions(2, 100, usgn, sgn, get_target));
  ASSERT_EQ(0, perform_conversions(5, 0, usgn, sgn, get_target));
  ASSERT_EQ(len - 2, perform_conversions(0, len - 2, usgn, sgn, get_target));
  ASSERT_EQ(0, perform_conversions(len - 1, 0, usgn, sgn, get_target));

  // Len now is 2^{ptr_size} - 1. (!)
  len = (1UL << ptr_size) - 1;
  target_get<ptr_size> new_gt(len);
  ASSERT_EQ(100, perform_conversions(0, 100, usgn, sgn, new_gt));
  ASSERT_EQ(len / 4 * 3, perform_conversions(0, len / 4 * 3, usgn, sgn, new_gt));
  ASSERT_EQ(0, perform_conversions(len / 4 * 3, 0, usgn, sgn, new_gt));
  ASSERT_EQ(1, perform_conversions(len - 1, 1, usgn, sgn, new_gt));
  ASSERT_EQ(len - 1, perform_conversions(1, len - 1, usgn, sgn, new_gt));
}

TYPED_TEST(Index, SerialCharacterAccess)
{
  using Alphabet = typename unpack<TypeParam>::Alphabet;
  using Symbol   = typename Alphabet::Symbol;
  auto idx = this->unload();
  auto source = this->source_get();
  std::vector<Symbol> got;
  for (auto i = 0U; i < source.size(); ++i) {
    got.push_back(this->index(i));
  }
  ASSERT_TRUE(check_eq(source, got));
}

TYPED_TEST(Index, SerialRangeAccess)
{
  using Alphabet = typename unpack<TypeParam>::Alphabet;
  using Symbol   = typename Alphabet::Symbol;

  auto idx = this->unload();

  auto source    = this->source_get();
  std::vector<Symbol> storage(source.size());
  for (auto start = 0U; start < source.size(); ++start) {
    for (auto end = start + 1; end <= source.size(); ++end) {
      this->index(start, end, storage.data());
      ASSERT_TRUE(check_eq(
        std::next(source.begin(), start),
        std::next(source.begin(), end),
        storage.begin(),
        std::next(storage.begin(), end - start)
      ));
      if (this->HasFatalFailure()) {
        std::cout << "Failure on range " << start << ", " << end << std::endl;
        std::cout << "Source = ";
        for (auto i : source) {
          std::cout << i << " ";
        }
        std::cout << std::endl;
        return;
      }
    }
  }
}


}}
