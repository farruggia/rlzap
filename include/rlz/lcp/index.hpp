#pragma once

#include "../index.hpp"
#include "index_iterator.hpp"

namespace rlz { namespace lcp {

template <
  typename Alphabet,
  typename Source,       // Source type. Must support operator[], begin() and end().
  typename ParseKeeper   = parse_keeper<Alphabet, values::Size<32>, values::Size<8>, vectors::dense, vectors::sparse>,
  typename LiteralKeeper = literal_split_keeper<Alphabet>
>
class index {
protected:
  Source                            source;
  ParseKeeper                       parse;
  LiteralKeeper                     literals;
  target_get<ParseKeeper::ptr_size> get_target;
public:
  using AlphabetType  = Alphabet;
  using Symbol        = typename Alphabet::Symbol;
  using ReferenceType = Source;
  using ReferenceIter = decltype(const_cast<const Source&>(source).begin());
  using size_type     = size_t;
  using iter          = iterator<Symbol, typename LiteralKeeper::iterator, ReferenceIter>;

  CHECK_TYPE_RANDOM(iter, "lcp::index: iterator is not random access");

  static_assert(
    type_utils::is_complaint<decltype(source[0]), Symbol>(),
    // std::is_same<type_utils::RemoveQualifiers<>, Symbol>::value, 
    "Source alphabet different from Index alphabet."
  );
  static_assert(
    type_utils::is_complaint<decltype(*(literals.literal_access(0))), Symbol>(),
    // std::is_same<type_utils::RemoveQualifiers<decltype(*(literals.literal_access(0)))>, Symbol>::value, 
    "Literal alphabet different from Index alphabet."
  );

  // Access functions
  template <typename OutputIt>
  void operator()(size_t begin, size_t end, OutputIt out) const
  {
    std::size_t phrase, subphrase;
    std::tie(phrase, subphrase) = parse.phrase_subphrase(begin);

    auto parse_iter = parse.get_iterator(phrase, subphrase);
    auto la_iter    = literals.literal_access(subphrase);
    auto ll_iter    = literals.get_iterator(subphrase);


    auto i  = begin;
    while (i != end) {
      std::size_t phrase_start;
      iter it, it_end;
      std::tie(phrase_start, it, it_end) = get_iterators(parse_iter, ll_iter, la_iter);
      
      std::advance(it, i - phrase_start);
      auto runs = std::min<std::size_t>(end - i, std::distance(it, it_end));
      it_end = std::next(it, runs);
      while (it != it_end) {
        *out++ = *it++;
      }

      ++parse_iter;
      la_iter += *ll_iter++;
      i += runs;
    }

    // for (auto i = begin; i != end; ++i) {
    //   *out++ = *it++;
    //   if (it == it_end) {
    //     ++parse_iter;
    //     la_iter += *ll_iter;
    //     ++ll_iter;
    //     std::tie(phrase_start, it, it_end) = get_iterators(parse_iter, ll_iter, la_iter);
    //   }
    // }
  }

  Symbol operator()(size_t idx) const
  {
    return (*this)(idx, idx + 1).front();
  }

  std::vector<Symbol> operator()(size_t begin, size_t end) const
  {
    std::vector<Symbol> to_ret(end - begin, Symbol{});
    (*this)(begin, end, to_ret.begin());
    return to_ret;
  }

  std::tuple<iter, iter, iter> iterator_position(std::size_t pos) const
  {
    std::size_t phrase, subphrase;
    std::tie(phrase, subphrase) = parse.phrase_subphrase(pos);

    auto parse_iter = parse.get_iterator(phrase, subphrase);
    auto la_iter    = literals.literal_access(subphrase);
    auto ll_iter    = literals.get_iterator(subphrase);

    std::size_t phrase_start;
    iter beg, end;
    std::tie(phrase_start, beg, end) = get_iterators(parse_iter, ll_iter, la_iter);

    return std::make_tuple(beg, std::next(beg, pos - phrase_start), end);
  }

  // Parse functions
  template <typename Func>
  void process_parsing(Func &f) const
  {
    auto parse_it  = parse.get_iterator_begin();
    auto lit_it    = literals.get_iterator(0UL);
    auto parse_end = parse.get_iterator_end();
    while (parse_it != parse_end) {
      std::int64_t offset;
      size_t phrase_start, phrase_len, lit_len;
      std::tie(phrase_start, offset, phrase_len) = *parse_it++;
      lit_len = *lit_it++;
      std::int64_t target = get_target(phrase_start + lit_len, offset);
      f(phrase_start, lit_len, target, phrase_len - lit_len);
    }
  }

  size_t size() const
  {
    return parse.length();
  }

  template <typename SourceT>
  void set_source(SourceT &&s)
  {
    source = std::forward<SourceT>(s);
    get_target.set_length(source.size());
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += parse.serialize(out, child, "parse");
    written_bytes += literals.serialize(out, child, "literals");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    parse.load(in);
    literals.load(in);
  }

  template <typename SymbolIt>
  class builder : public rlz::lcp::build::observer<Alphabet, SymbolIt> {
    using Symbol        = typename Alphabet::Symbol;
    using Index         = index<Alphabet, Source,ParseKeeper, LiteralKeeper>;
    using LitBuilder    = typename LiteralKeeper::template lcp_builder<SymbolIt>;
    using ParseBuilder  = typename ParseKeeper::template lcp_builder<SymbolIt>;

    LitBuilder    lit_build;
    ParseBuilder  parse_build;
  public:
    builder() { }

    std::size_t can_split(
      std::size_t source,
      const SymbolIt bytes, std::size_t junk_len,
      std::size_t target, std::size_t len
    ) override
    {
      return std::min(
        lit_build.can_split(source, bytes, junk_len, target, len),
        parse_build.can_split(source, bytes, junk_len, target, len)
      );
    }

    // Returns true if splitting here allocates an another block.
    bool split_as_block(
      std::size_t source,
      const SymbolIt bytes, std::size_t junk_len,
      std::size_t target, std::size_t len
    ) override
    {
      return lit_build.split_as_block(source, bytes, junk_len, target, len) or 
             parse_build.split_as_block(source, bytes, junk_len, target, len);
    }

    // Splits at given phrase.
    void split(
      std::size_t source,
      const SymbolIt bytes, std::size_t junk_len,
      std::size_t target, std::size_t len,
      bool block_split
    ) override
    {
      lit_build.split(source, bytes, junk_len, target, len, block_split);
      parse_build.split(source, bytes, junk_len, target, len, block_split);
    }

    // Parsing finished
    void finish() override
    {
      lit_build.finish();
      parse_build.finish();
    }

    Index get(const Source &s)
    {
      Index idx;
      idx.literals   = lit_build.get();
      idx.parse      = parse_build.get();
      idx.set_source(s);
      return idx;
    }
  };
private:
  template <typename ParseIter, typename LiteralLen, typename LiteralAccess>
  std::tuple<std::size_t, iter, iter> get_iterators(ParseIter parse_iter, LiteralLen ll_iter, LiteralAccess la_iter) const
  {
    if (parse_iter == parse.get_iterator_end()) {
      return std::make_tuple(
        this->size(), iter(), iter()
      );
    }
    std::ptrdiff_t offset;
    std::size_t phrase_start, phrase_len;
    std::tie(phrase_start, offset, phrase_len) = *parse_iter;
    auto lit_len   = *ll_iter;
    auto lit_begin = la_iter;
    auto lit_end   = std::next(lit_begin, lit_len);
    
    auto target = get_target(phrase_start + lit_len, offset);

    auto ref_begin = std::next(source.begin(), target);
    auto copy_len  = phrase_len - lit_len;

    Symbol prev_ref, prev_source;

    if (lit_begin == lit_end) {
      prev_ref = prev_source = *ref_begin;
    } else if (copy_len > 0) {
      assert(copy_len <= source.size());
      prev_ref    = ref_begin == source.begin() ? Symbol{} : *std::prev(ref_begin);
      prev_source = *std::prev(lit_end);
    }

    return std::make_tuple(
      phrase_start,
      iter(lit_begin, lit_end, ref_begin, 0UL, copy_len, prev_source, prev_ref),
      iter(lit_begin, lit_end, ref_begin, phrase_len, copy_len, prev_source, prev_ref)
    );
  }
};

}}