#pragma once
#include <fstream>
#include <istream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "alphabet.hpp"
#include "classic_parse_keeper.hpp"
#include "containers.hpp"
#include "dumper.hpp"
#include "impl/api.hpp"
#include "io.hpp"
#include "trivial_prefix.hpp"
#include "type_utils.hpp"

namespace rlz {

namespace api {
  template <
    typename PtrSize = values::Size<32UL>, 
    typename DiffSize = values::Size<2UL>, 
    typename PhraseBVRep = vectors::dense, 
    typename SubphraseBVRep = vectors::sparse
  >
  struct ParseKeeper {
    template <typename Alphabet>
    using Type = parse_keeper<Alphabet, PtrSize, DiffSize, PhraseBVRep, SubphraseBVRep>;
  };

  template <
    typename PtrSize = values::Size<32UL>, 
    typename PhraseBVRep = vectors::dense, 
    typename SubphraseBVRep = vectors::sparse
  >
  struct ClassicParseKeeper {
    template <typename Alphabet>
    using Type = rlz::classic::parse_keeper<Alphabet, PtrSize, PhraseBVRep, SubphraseBVRep>;
  };

  template <
    typename Prefix = prefix::cumulative<vectors::sparse, values::Size<8U>>
  >
  struct LiteralKeeper {
    template <typename Alphabet>
    using Type = literal_split_keeper<Alphabet, Prefix>;
  };
}

//////////////////////////////////// BUILD FUNCTIONS ///////////////////////////////////////////
// Reference must support operator[], begin(), end(), size() (all marked const) and a default constructor.

// Construct index starting from input (pointer to start), a Reference instance and matching statistics (pair of iterators)
template <
  typename Alphabet, 
  typename ParseKeeper   = api::ParseKeeper<>, 
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType,
  typename Reference,
  typename MatchIt
>
impl::Index<Alphabet, container_wrapper<Alphabet, Reference>, ParseKeeper, LiteralKeeper> construct_ptr(
  typename Alphabet::Symbol *input, Reference reference, ParseType parser,
  MatchIt match_begin, MatchIt match_end
)
{
  // Return index
  using PK = impl::Bind<ParseKeeper, Alphabet>;
  using LK = impl::Bind<LiteralKeeper, Alphabet>;
  return impl::construct<Alphabet, PK, LK>(
    input,
    container_wrapper<Alphabet, Reference>(std::move(reference)), 
    parser,
    match_begin, match_end
  );
}

// Construct index starting from input (iterator pairs) and a Reference instance.
template <
  typename Alphabet, 
  typename ParseKeeper   = api::ParseKeeper<>, 
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType,
  typename Reference,
  typename InputIt
>
impl::Index<Alphabet, container_wrapper<Alphabet, Reference>, ParseKeeper, LiteralKeeper> construct_iterator(
  InputIt input_begin, InputIt input_end, Reference reference, ParseType parser
)
{

  // Get matching stats
  auto reference_dump = rlz::utils::get_iterator_dumper(reference.begin(), reference.end());
  auto input_dump     = rlz::utils::get_iterator_dumper(input_begin, input_end);
  std::vector<match> matches;
  mapped_stream<Alphabet> reference_ms, input_ms;
  std::tie(matches, reference_ms, input_ms) = get_relative_matches<Alphabet>(reference_dump, input_dump);

  // Return index
  using PK = impl::Bind<ParseKeeper, Alphabet>;
  using LK = impl::Bind<LiteralKeeper, Alphabet>;
  return impl::construct<Alphabet, PK, LK>(
    input_ms.data(), 
    container_wrapper<Alphabet, Reference>(std::move(reference)),
    parser,
    matches.begin(), matches.end()
  );
}

// Construct index starting from input (input stream) and a Reference instance.
template <
  typename Alphabet, 
  typename ParseKeeper   = api::ParseKeeper<>, 
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType,
  typename Reference
>
impl::Index<Alphabet, container_wrapper<Alphabet, Reference>, ParseKeeper, LiteralKeeper> construct_stream(
  std::istream &input, Reference reference, ParseType parser
)
{
  // Check streams
  if (!input.good()) {
    throw std::logic_error("Input stream not readable");
  }

  // Get matching stats
  rlz::utils::stream_dumper input_dump(input);
  auto ref_dump = rlz::utils::get_iterator_dumper(reference.begin(), reference.end());
  std::vector<match> matches;
  mapped_stream<Alphabet> reference_ms, input_ms;
  std::tie(matches, reference_ms, input_ms) = get_relative_matches<Alphabet>(ref_dump, input_dump);
  
  using PK = impl::Bind<ParseKeeper, Alphabet>;
  using LK = impl::Bind<LiteralKeeper, Alphabet>;
  return impl::construct<Alphabet, PK, LK>(
    input_ms.data(), 
    container_wrapper<Alphabet, Reference>(std::move(reference)),
    parser,
    matches.begin(), matches.end()
  );
}

// Build index, with input and reference, both provided as input streams.
template <
  typename Alphabet, 
  typename ParseKeeper   = api::ParseKeeper<>,
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType
>
impl::Index<Alphabet, mapped_stream<Alphabet>, ParseKeeper, LiteralKeeper> construct_sstream(
  std::istream &input, std::istream &reference, ParseType parser
)
{
  if (!input) {
    throw std::logic_error("Input stream not readable");
  }
  if (!reference) {
    throw std::logic_error("Reference stream not readable");
  }

  // Get matching stats
  rlz::utils::stream_dumper input_dump(input), ref_dump(reference);
  std::vector<match> matches;
  mapped_stream<Alphabet> reference_ms, input_ms;
  std::tie(matches, reference_ms, input_ms) = get_relative_matches<Alphabet>(ref_dump, input_dump);
  using PK = impl::Bind<ParseKeeper, Alphabet>;
  using LK = impl::Bind<LiteralKeeper, Alphabet>;

  return impl::construct<Alphabet, PK, LK>(
    input_ms.data(), reference_ms,
    parser, matches.begin(), matches.end()
  );
}

// Build index, with input and reference provided as input streams and precomputed matching stats.
template <
  typename Alphabet,
  typename ParseKeeper   = api::ParseKeeper<>,
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType,
  typename MatchIt
>
impl::Index<Alphabet, managed_wrap<Alphabet>, ParseKeeper, LiteralKeeper> construct_sstream(
  std::istream &input, std::istream &reference,
  ParseType parser,
  MatchIt m_begin, MatchIt m_end
)
{
  using Symbol = typename Alphabet::Symbol;
  // Check streams
  if (!input.good()) {
    throw std::logic_error("Input stream not readable");
  }
  if (!reference.good()) {
    throw std::logic_error("Reference stream not readable");
  }

  // Read input
  auto input_data = rlz::io::read_stream<Symbol>(input);
  auto input_ptr  = input_data.get();

  // Read reference
  size_t reference_length;
  auto reference_uniq = rlz::io::read_stream<Symbol>(reference, &reference_length);
  std::shared_ptr<Symbol> reference_shared { reference_uniq.release(), std::default_delete<Symbol[]>{} };
  managed_wrap<Alphabet> reference_wrap { reference_shared, reference_length };

  using PK = impl::Bind<ParseKeeper, Alphabet>;
  using LK = impl::Bind<LiteralKeeper, Alphabet>;

  return impl::construct<Alphabet, PK, LK>(
    input_ptr, reference_wrap,
    parser,
    m_begin, m_end
  );
}

// Build index, with input and reference provided as input streams and precomputed matching stats and precomputed matching statistics.
template <
  typename Alphabet, 
  typename ParseKeeper   = api::ParseKeeper<>,
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType
>
impl::Index<Alphabet, mapped_stream<Alphabet>, ParseKeeper, LiteralKeeper> construct_sstream(
  const char *input_name, const char *reference_name, ParseType parser
)
{
  std::ifstream input(input_name, std::ifstream::in);
  std::ifstream reference(reference_name, std::ifstream::in);

  return construct_sstream<Alphabet, ParseKeeper, LiteralKeeper>(input, reference, parser);
}

// Build index, 
template <
  typename Alphabet, 
  typename ParseKeeper   = api::ParseKeeper<>,
  typename LiteralKeeper = api::LiteralKeeper<>,
  typename ParseType,
  typename MatchIt
>
impl::Index<Alphabet, managed_wrap<Alphabet>, ParseKeeper, LiteralKeeper> construct_sstream(
  const char *input_name, const char *reference_name, 
  ParseType parser,
  MatchIt m_begin, MatchIt m_end
)
{
  std::ifstream input(input_name, std::ifstream::in);
  std::ifstream reference(reference_name, std::ifstream::in);

  return construct_sstream<Alphabet, ParseKeeper, LiteralKeeper>(input, reference, parser, m_begin, m_end);
}


///////////////////////////////// SERIALIZATION SUPPORT ////////////////////////////////////////
namespace serialize {

namespace supported {

//////// Register types enabled to be serialized ////////

// Alphabets
using alphabets      = type_utils::type_list<rlz::alphabet::dlcp_32, rlz::alphabet::dna<>>;

// Bit-Vector literal prefix
using bv             = type_utils::type_list<rlz::vectors::dense>;
using literal_length = type_utils::type_list<values::Size<2U>, values::Size<4U>, values::Size<8U>>;
using cum_prefix     = type_utils::Prod<rlz::prefix::cumulative, bv, literal_length>::type;

// Sample literal prefix
using sample_intvals = type_utils::type_list<values::Size<16U>, values::Size<32U>, values::Size<48U>, values::Size<64U>>;
using sample_prefix  = type_utils::Prod<rlz::prefix::sampling_cumulative, literal_length, sample_intvals>::type;

// Prefix choice: BV + Sample
using prefix         = type_utils::join_lists<cum_prefix, sample_prefix>::type;

// Literal: instantiate LiteralKeeper with all prefixes
using literal        = type_utils::WInst<api::LiteralKeeper, prefix>::type;

// ParseKeeper choices: ptrs + diffs
using ptr_sizes      = type_utils::type_list<values::Size<32UL>>;
using diff_sizes     = type_utils::type_list<values::Size<2UL>, values::Size<4UL>, values::Size<6UL>, values::Size<8UL>/*, values::Size<16UL>*/>;
using rlzap_parse    = type_utils::Prod<api::ParseKeeper, ptr_sizes, diff_sizes>::type;

// RLZAP configurations: parsing + literal + alphabets
using rlzap_confs    = type_utils::Prod<type_utils::type_list, rlzap_parse, literal, alphabets>::type;

// RLZ: classic parse + classic literal (fake), on all alphabets
using parse_classic  = type_utils::Prod<api::ClassicParseKeeper, ptr_sizes>::type;
using lit_classic    = type_utils::type_list<api::LiteralKeeper<rlz::classic::prefix::trivial>>;
using rlz_confs      = type_utils::Prod<type_utils::type_list, parse_classic, lit_classic, alphabets>::type;

// All configurations: old + new
using configurations = type_utils::join_lists<rlzap_confs, rlz_confs>::type;
}

// Define machinery to invoke functions with supported type
template <typename T>
class invoke_id { };

template <>
class invoke_id<type_utils::type_list<>> {
public:

  template <typename Call>
  void call_id(Call &c, size_t id_w)
  {
    throw std::logic_error("No index found with id " + std::to_string(id_w));
  }

  enum ID { id = 0 };
};

template <typename T, typename IndexAlphabet, typename IndexParse, typename IndexLiteral>
struct check { };

template <
  typename IndexAlphabet, 
  typename IndexParse, 
  typename IndexLiteral, 
  typename ListAlphabet,
  typename ListParseMask,
  typename ListLiteralMask>
struct check<type_utils::type_list<ListParseMask, ListLiteralMask, ListAlphabet>, IndexAlphabet, IndexParse, IndexLiteral> {
  static constexpr bool value = 
    std::is_same<ListAlphabet, IndexAlphabet>::value                              and 
    std::is_same<impl::Bind<ListParseMask, ListAlphabet>, IndexParse>::value      and
    std::is_same<impl::Bind<ListLiteralMask, ListAlphabet>, IndexLiteral>::value;
};

template <typename T>
struct caller;

template <typename Alphabet, typename ParseMask, typename LiteralMask>
struct caller<type_utils::type_list<ParseMask, LiteralMask, Alphabet>> {
  template <typename Call>
  void invoke(Call &c) {
    c.template invoke<Alphabet, impl::Bind<ParseMask, Alphabet>, impl::Bind<LiteralMask, Alphabet>>();
  }
};

template <typename Head, typename... Tail>
class invoke_id<type_utils::type_list<Head, Tail...>> {
public:
  template <typename IndexAlphabet, typename IndexParse, typename IndexLiteral, typename Call>
  auto call_type(Call &c) -> typename std::enable_if<check<Head, IndexAlphabet, IndexParse, IndexLiteral>::value>::type
  {
      c(id);
  }

  template <typename IndexAlphabet, typename IndexParse, typename IndexLiteral, typename Call>
  auto call_type(Call &c) -> typename std::enable_if<!check<Head, IndexAlphabet, IndexParse, IndexLiteral>::value>::type
  {
      invoke_id<type_utils::type_list<Tail...>>{}.template call_type<IndexAlphabet, IndexParse, IndexLiteral>(c);
  }

  template <typename Call>
  void call_id(Call &c, size_t id_w) {
    if (id == id_w) {
      caller<Head>{}.invoke(c);
    } else {
      invoke_id<type_utils::type_list<Tail...>>{}.call_id(c, id_w);
    }
  }

  enum ID { id = invoke_id<type_utils::type_list<Tail...>>::id + 1 };
};

using InvokeId = invoke_id<supported::configurations>;

// ID serialization function
struct Id {
  std::uint32_t value;

  Id() : value(0U) { };

  Id(size_t value) : value(value) { }

  operator unsigned int() { return value; }

  void save(std::ostream &s)
  {
    char *v_ptr = reinterpret_cast<char*>(&value);
    s.write(v_ptr, sizeof(value));
  }

  static Id load(std::istream &s)
  {
    Id id;
    char *v_ptr = reinterpret_cast<char*>(&(id.value));
    s.read(v_ptr, sizeof(id.value));
    return id;
  }

};

//////////////////////////////////// STORE FUNCTIONS ///////////////////////////////////////////
template <typename Index>
struct store_obj { };

template <typename Alphabet, typename Source, typename Parse, typename Literal>
struct store_obj<index<Alphabet, Source, Parse, Literal>> {
  void operator()(const index<Alphabet, Source, Parse, Literal> &idx, std::ostream &stream)
  {
    auto do_work = [&] (size_t id) {
      Id(id).save(stream);
    };
    InvokeId{}.call_type<Alphabet, Parse, Literal>(do_work);
    idx.serialize(stream);    
  }
};

template <typename Index>
void store(const Index &idx, std::ostream &stream)
{
  store_obj<Index>{}(idx, stream);
}

template <typename Index>
void store(const Index &idx, const char *out_file)
{
  std::ofstream out(out_file, std::ofstream::out);
  store(idx, out);
}

//////////////////////////////////// LOAD FUNCTIONS ////////////////////////////////////////////
template<typename Call, typename ReferenceFactory>
class loader {
private:
  std::istream &stream;
  Call &c;
  ReferenceFactory &ref;
public:
  loader(std::istream &stream, Call &c, ReferenceFactory &ref)
    : stream(stream), c(c), ref(ref)
  {

  }

  template <typename IndexAlphabet, typename IndexParse, typename IndexLiteral>
  void invoke()
  {
    using Reference = typename std::remove_cv<decltype(ref.template get<IndexAlphabet>())>::type;
    using Index = index<IndexAlphabet, Reference, IndexParse, IndexLiteral>;
    Index idx;
    idx.load(stream);
    idx.set_source(ref.template get<IndexAlphabet>());
    c.template invoke<Index>(idx); // In C++14 we can use generic lambdas...
  }
};

template <typename ReferenceFactory, typename Call>
void load_factory(std::istream &stream, ReferenceFactory &ref, Call &c)
{
  // Check stream
  if (!stream.good()) {
    throw std::logic_error("Input stream not readable");
  }

  std::uint32_t id = Id::load(stream);
  loader<Call, ReferenceFactory> load(stream, c, ref);
  InvokeId{}.call_id(load, id);
}

template <typename ReferenceFactory, typename Call>
void load_factory(const char *file_name, ReferenceFactory &ref, Call &c)
{
  std::ifstream in(file_name, std::ofstream::in);
  load_factory(in, ref, c);
}

template <typename Alphabet, typename Reference, typename Call>
void load_reference(std::istream &stream, Reference ref, Call &c)
{
  impl::getter_factory<Alphabet, Reference> factory { std::move(ref) };
  load_factory(stream, factory, c);
}

template <typename Alphabet, typename Reference, typename Call>
void load_reference(const char *file_name, Reference ref, Call &c)
{
  impl::getter_factory<Alphabet, Reference> factory { std::move(ref) };
  load_factory(file_name, factory, c);
}

template <typename Call>
void load_stream(std::istream &index, std::istream &reference, Call &c)
{
  impl::stream_factory factory{reference};
  load_factory(index, factory, c);
}

template <typename Call>
void load_stream(const char *index_name, const char *reference_name, Call &c)
{
  std::ifstream index(index_name, std::ofstream::in);
  std::ifstream reference(reference_name, std::ofstream::in);
  load_stream(index, reference, c);
}

template <typename Alphabet, typename Iterator, typename Call>
void load_iterator(std::istream &index, Iterator ref_begin, Iterator ref_end, Call &c)
{
  impl::iterator_factory<Alphabet, Iterator> factory{ref_begin, ref_end};
  load_factory(index, factory, c);
}

template <typename Alphabet, typename Iterator, typename Call>
void load_iterator(const char *index_filename, Iterator ref_begin, Iterator ref_end, Call &c)
{
  std::ifstream index(index_filename, std::ofstream::in);
  load_iterator<Alphabet>(index, ref_begin, ref_end, c);
}

}

}
