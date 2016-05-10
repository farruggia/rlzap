#include <alphabet.hpp>
#include <api.hpp>
#include <classic_parse.hpp>
#include <classic_parse_keeper.hpp>
#include <containers.hpp>
#include <dumper.hpp>
#include <parse.hpp>
#include <parse_rlzap.hpp>
#include <parse_keeper.hpp>
#include <get_matchings.hpp>

#include <iterator>
#include <sstream>
#include <vector>

#include "main.hpp"

std::string reference_char = "AACTTCAGGTGTCTTTGATGGAATCCTATTGGTAAAAAATTCAGGTAACGATTGAACTTCAATGGAATGAATTTTTTCTAAATTGAACCATTTAGAATAACTTGGAATAACAATTTCATGTGCTTGCGGTATCTCAAAAGTTTCGGGTTC";
std::string source_char    = "AATCCTATTGGTAAAAAATTCAGGTAACAACTTCAGGTGTCTTTGATGTTCATGTGCTTGCGGTATCTCAAAAGTTTCGGGTTCAAAAAAAAAAAAAAAAAAAAAAAAAAAATTCTAAATTGAACCATTTAGAATAACTTGGCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCATTGGTAAAAAATTCAGGTAACGATT";

using namespace rlz;
using type_utils::type_list;

template <typename T>
struct type_getter { };

template <typename ParseKeeper, typename LiteralKeeper, typename AlphabetType>
struct type_getter<type_list<ParseKeeper, LiteralKeeper, AlphabetType>> {
  using Alphabet  = AlphabetType;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = ParseKeeper;
  using Literal   = LiteralKeeper;
  using Iterator  = typename std::vector<Symbol>::iterator;
  using Reference = iterator_container<Alphabet, Iterator>;
  using Index     = rlz::index<
                      Alphabet, 
                      Reference, 
                      impl::Bind<ParseKeeper, Alphabet>, 
                      impl::Bind<LiteralKeeper, Alphabet>
                    >;
};

template <typename Alphabet>
struct refinput_get { };

template <typename Parm>
struct refinput_get<rlz::alphabet::dna<Parm>> {
  std::vector<char> reference()
  {
    return std::vector<char>(reference_char.begin(), reference_char.end());
  }

  std::vector<char> input()
  {
    return std::vector<char>(source_char.begin(), source_char.end());
  }
};

template <typename IntSize>
struct refinput_get<rlz::alphabet::integer<IntSize>>
{
  using T = typename rlz::alphabet::integer<IntSize>::Symbol;
  std::vector<T> reference()
  {
    return std::vector<T>(reference_char.begin(), reference_char.end());
  }

  std::vector<T> input()
  {
    return std::vector<T>(source_char.begin(), source_char.end());
  }
};

template <typename IntSize>
struct refinput_get<rlz::alphabet::lcp<IntSize>>
{
  using T = typename rlz::alphabet::lcp<IntSize>::Symbol;
  std::vector<T> reference()
  {
    auto to_ret = std::vector<T>(reference_char.begin(), reference_char.end());
    for (auto &i : to_ret) {
      if (i == static_cast<T>('N')) {
        i = 4242;
      }
    }    
    return to_ret;
  }

  std::vector<T> input()
  {
    auto to_ret = std::vector<T>(source_char.begin(), source_char.end());
    for (auto &i : to_ret) {
      if (i == static_cast<T>('N')) {
        i = 4242;
      }
    }
    return to_ret;
  }
};

template <typename ConfigTuple>
class Api : public ::testing::Test {
public:
  using Alphabet  = typename type_getter<ConfigTuple>::Alphabet;
  using Symbol    = typename type_getter<ConfigTuple>::Symbol;
  using Parse     = typename type_getter<ConfigTuple>::Parse;
  using Literal   = typename type_getter<ConfigTuple>::Literal;
  using Iterator  = typename type_getter<ConfigTuple>::Iterator;
  using Reference = typename type_getter<ConfigTuple>::Reference;
  using Index     = typename type_getter<ConfigTuple>::Index;

  std::vector<Symbol> reference;
  std::vector<Symbol> input;

  std::vector<Symbol> reference_get()
  {
    return reference;
  }

  std::vector<Symbol> input_get()
  {
    return input;
  }

  Reference reference_container()
  {
    return Reference(reference.begin(), reference.end());
  }

  virtual void SetUp() {
    reference = refinput_get<Alphabet>{}.reference();
    input     = refinput_get<Alphabet>{}.input();
  }
};

template <typename T>
struct convert_type { };

template <typename... T>
struct convert_type<type_list<T...>> {
  using type = ::testing::Types<T...>;
};

template <typename Configs, typename Picked, size_t Counter, size_t Gap>
struct pick { };

template <typename ConfHead, typename... ConfTail, typename... Picked, size_t Counter, size_t Gap>
struct pick<type_list<ConfHead, ConfTail...>, type_list<Picked...>, Counter, Gap>
{
  using Type = typename pick<type_list<ConfTail...>, type_list<Picked...>, Counter - 1, Gap>::Type;
};

template <typename ConfHead, typename... ConfTail, typename... Picked, size_t Gap>
struct pick<type_list<ConfHead, ConfTail...>, type_list<Picked...>, 0UL, Gap>
{
  using Type = typename pick<type_list<ConfTail...>, type_list<Picked..., ConfHead>, Gap - 1, Gap>::Type;
};

template <typename... Picked, size_t Counter, size_t Gap>
struct pick<type_list<>, type_list<Picked...>, Counter, Gap>
{
  using Type = type_list<Picked...>;
};

template <typename List, size_t Counter>
using Pick = typename pick<List, type_list<>, 0UL, Counter>::Type;

constexpr size_t no_types    = rlz::serialize::InvokeId::id;
constexpr size_t conf_to_try = 10UL;
constexpr size_t gap         = no_types / conf_to_try;
using limited  = Pick<serialize::supported::configurations, gap>;
using ApiTypes = convert_type<limited>::type;

TYPED_TEST_CASE(Api, ApiTypes);

template <typename It1, typename It2>
void check_iterable(const It1 &exp, const It2 &got)
{
  ASSERT_EQ(exp.size(), got.size());
  for (auto i = 0U; i < exp.size(); ++i) {
    ASSERT_EQ(exp[i], got[i]);
  }
}

template <typename ParseKeeperType>
struct GetParserType {
};

template <typename P, typename D, typename Ph, typename Su>
struct GetParserType<api::ParseKeeper<P, D, Ph, Su>> {
  using Type = rlz::parser_rlzap;
};

template <typename P, typename Ph, typename Su>
struct GetParserType<api::ClassicParseKeeper<P, Ph, Su>> {
  using Type = rlz::classic::parser;
};

template <typename PKType>
using ProperParser = typename GetParserType<PKType>::Type;


TYPED_TEST(Api, ConstructRefIterator)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref_cont  = this->reference_container();
  auto index = construct_iterator<Alphabet, Parse, Literal>(input.begin(), input.end(), ref_cont, ProperParser<Parse>{});
  std::vector<Symbol> output;
  index(0UL, index.size(), std::back_inserter(output));
  check_iterable(input, output);
}

TYPED_TEST(Api, ConstructRefStream)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref_cont  = this->reference_container();
  std::string input_s(reinterpret_cast<char*>(input.data()), input.size() * sizeof(Symbol));
  std::istringstream ss(input_s);
  auto index = construct_stream<Alphabet, Parse, Literal>(ss, ref_cont, ProperParser<Parse>{});
  std::vector<Symbol> output;
  index(0UL, index.size(), std::back_inserter(output));
  check_iterable(input, output);
}

TYPED_TEST(Api, ConstructStreamStream)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref       = this->reference_get();
  std::string input_s(reinterpret_cast<char*>(input.data()), input.size() * sizeof(Symbol));
  std::istringstream input_ss(input_s);
  std::string reference_s(reinterpret_cast<char*>(ref.data()), ref.size() * sizeof(Symbol));
  std::istringstream reference_ss(reference_s);
  auto index = construct_sstream<Alphabet, Parse, Literal>(input_ss, reference_ss, ProperParser<Parse>{});
  std::vector<Symbol> output;
  index(0UL, index.size(), std::back_inserter(output));
  check_iterable(input, output);
}

TYPED_TEST(Api, ConstructAcceleratedStream)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref       = this->reference_get();
  std::string input_s(reinterpret_cast<char*>(input.data()), input.size() * sizeof(Symbol));
  std::istringstream input_ss(input_s);
  std::string reference_s(reinterpret_cast<char*>(ref.data()), ref.size() * sizeof(Symbol));
  std::istringstream reference_ss(reference_s);

  std::vector<rlz::match> matches;
  {
    auto ref_dump   = rlz::utils::get_iterator_dumper(ref.begin(), ref.end());
    auto input_dump = rlz::utils::get_iterator_dumper(input.begin(), input.end());
    matches = std::get<0>(rlz::get_relative_matches<Alphabet>(ref_dump, input_dump));
  }
  auto index = construct_sstream<Alphabet, Parse, Literal>(
    input_ss, reference_ss, ProperParser<Parse>{}, matches.begin(), matches.end()
  );
  std::vector<Symbol> output;
  index(0UL, index.size(), std::back_inserter(output));
  check_iterable(input, output);
}

TYPED_TEST(Api, ConstructAcceleratedPtr)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref       = this->reference_get();
  auto ref_cont  = this->reference_container();

  std::vector<rlz::match> matches;
  {
    auto ref_dump   = rlz::utils::get_iterator_dumper(ref.begin(), ref.end());
    auto input_dump = rlz::utils::get_iterator_dumper(input.begin(), input.end());
    matches = std::get<0>(rlz::get_relative_matches<Alphabet>(ref_dump, input_dump));
  }
  auto index = construct_ptr<Alphabet, Parse, Literal>(
    input.data(), ref_cont, ProperParser<Parse>{}, matches.begin(), matches.end()
  );
  std::vector<Symbol> output;
  index(0UL, index.size(), std::back_inserter(output));
  check_iterable(input, output);
}

template <typename Alphabet>
struct UseLoad {

  using Symbol = typename Alphabet::Symbol;
  const std::vector<Symbol> input;

  UseLoad(const std::vector<Symbol> input) : input(input) { }

  template <typename Index>
  void invoke(Index &index)
  {
    using Symbol = typename Alphabet::Symbol;
    std::vector<Symbol> output;
    index(0UL, index.size(), std::back_inserter(output));
    check_iterable(input, output);
  }
};

TYPED_TEST(Api, LoadIteratorStore)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref       = this->reference_get();
  auto ref_cont  = this->reference_container();
  auto index = construct_iterator<Alphabet, Parse, Literal>(input.begin(), input.end(), ref_cont, ProperParser<Parse>{});

  std::stringstream stored;
  serialize::store(index, stored);
  UseLoad<Alphabet> caller(input);
  serialize::load_iterator<Alphabet>(stored, ref.begin(), ref.end(), caller);
}

TYPED_TEST(Api, LoadReferenceStore)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref       = this->reference_get();
  auto ref_cont  = this->reference_container();
  auto index = construct_iterator<Alphabet, Parse, Literal>(input.begin(), input.end(), ref_cont, ProperParser<Parse>{});

  std::stringstream stored;
  serialize::store(index, stored);
  UseLoad<Alphabet> caller(input);
  serialize::load_reference<Alphabet>(stored, ref_cont, caller);
}

TYPED_TEST(Api, LoadStreamStore)
{
  using Alphabet  = typename Api<TypeParam>::Alphabet;
  using Symbol    = typename Alphabet::Symbol;
  using Parse     = typename Api<TypeParam>::Parse;
  using Literal   = typename Api<TypeParam>::Literal;
  auto input     = this->input_get();
  auto ref       = this->reference_get();
  auto ref_cont  = this->reference_container();
  auto index = construct_iterator<Alphabet, Parse, Literal>(input.begin(), input.end(), ref_cont, ProperParser<Parse>{});

  std::stringstream stored;
  serialize::store(index, stored);

  std::string reference_s(reinterpret_cast<char*>(ref.data()), ref.size() * sizeof(Symbol));
  std::istringstream reference_ss(reference_s);
  UseLoad<Alphabet> caller(input);
  serialize::load_stream(stored, reference_ss, caller);
}
