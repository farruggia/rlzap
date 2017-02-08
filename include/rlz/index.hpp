#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include <sdsl/util.hpp>

#include "integer_type.hpp"
#include "parse_keeper.hpp"
#include "literal_keeper.hpp"
#include "type_name.hpp"
#include "type_utils.hpp"

namespace rlz {

template <size_t Width>
class target_get {
private:
  size_t length;
  static constexpr std::uint64_t xor_mask = 0xFFFFFFFFFFFFFFFF - ((1ULL << Width) - 1);
public:

  target_get() : length(0UL) { }

  target_get(size_t length) : length(length) { }

  void set_length(size_t length)
  {
    *this = target_get(length);
  }

  size_t operator()(size_t pos, std::int64_t ptr) const
  {
    size_t target = pos + ptr;
    size_t v[2]   = { target, pos + (ptr ^ xor_mask) };
    return v[target > length];
  }
};

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
  using size_type     = size_t;
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
  void operator()(size_t begin, size_t end, OutputIt out) const;
  Symbol operator()(size_t idx) const;
  std::vector<Symbol> operator()(size_t begin, size_t end) const;

  // Parse functions
  template <typename Func>
  void process_parsing(Func &f) const
  {
    auto parse_it  = parse.get_iterator_begin();
    auto lit_it    = literals.get_iterator(0UL);
    auto parse_end = parse.get_iterator_end();
    while (parse_it != parse_end) {
      std::int64_t offset;
      size_t phrase_start, copy_len, lit_len;
      std::tie(phrase_start, offset, copy_len) = *parse_it++;
      std::int64_t target = get_target(phrase_start, offset);
      lit_len = *lit_it++;
      f(target - phrase_start, copy_len, lit_len);
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
  class builder : public rlz::build::observer<Alphabet, SymbolIt> {
    using Symbol        = typename Alphabet::Symbol;
    using Index         = index<Alphabet, Source,ParseKeeper, LiteralKeeper>;
    using LitBuilder    = typename LiteralKeeper::template builder<SymbolIt>;
    using ParseBuilder  = typename ParseKeeper::template builder<SymbolIt>;

    LitBuilder    lit_build;
    ParseBuilder  parse_build;
  public:
    builder() { }

    size_t can_split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt chars) override
    {
      return std::min(
        lit_build.can_split(source, target, len, junk_len, chars),
        parse_build.can_split(source, target, len, junk_len, chars)
      );
    }

    // Returns true if splitting here allocates an another block.
    bool split_as_block(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt chars) override
    {
      return lit_build.split_as_block(source, target, len, junk_len, chars) or 
             parse_build.split_as_block(source, target, len, junk_len, chars);
    }


    // Splits at given phrase.
    void split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt bytes, bool new_block) override
    {
      lit_build.split(source, target, len, junk_len, bytes, new_block);
      parse_build.split(source, target, len, junk_len, bytes, new_block);
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
      idx.source     = s;
      idx.get_target = target_get<ParseKeeper::ptr_size>(idx.source.size());
      return idx;
    }
  };
};

template <typename Alphabet, typename Source, typename ParseKeeper, typename LiteralKeeper>
template <typename OutputIt>
void index<Alphabet, Source, ParseKeeper, LiteralKeeper>::operator()(size_t begin, size_t end, OutputIt output) const
{
  size_t current_pos = begin;
  auto remaining     = end - current_pos;
  // Get (phrase, subphrase)
  size_t phrase, subphrase;
  std::tie(phrase, subphrase) = parse.phrase_subphrase(begin);

  // Get iterators
  auto parse_it = parse.get_iterator(phrase, subphrase);
  size_t        start_copy;
  std::int64_t  ptr;
  size_t        copy_len;
  std::tie(start_copy, ptr, copy_len) = *parse_it++;

  // Fast case: no literal computation
  if (end + LiteralKeeper::max_literal_length <= start_copy + copy_len) {
    auto target_off = get_target(begin, ptr);
    auto target_beg = std::next(source.begin(), target_off);
    auto target_end = std::next(target_beg, remaining);
    std::copy(target_beg, target_end, output);
    return;
  }

  // Fetch literals and perform the standard computation
  size_t lit_len;
  auto   len_it = literals.get_iterator(subphrase);
  auto   lit_it = literals.literal_access(subphrase);
  lit_len       = *len_it++;
  copy_len     -= lit_len;

  // Adjust current phrase by accounting starting position
  {
    auto end_copy   = start_copy + copy_len;
    if (current_pos < end_copy) {
      copy_len = end_copy - current_pos;
    } else {
      auto end_phrase = end_copy + lit_len;
      copy_len = 0U;
      lit_len  = end_phrase - current_pos;
      std::advance(lit_it, current_pos - end_copy);
    }
  }

  while (remaining > 0) {
    // Copy
    copy_len          = std::min(copy_len, remaining);
    auto target_off   = get_target(current_pos, ptr);
    auto target_beg   = std::next(source.begin(), target_off);
    auto target_end   = std::next(target_beg, copy_len);
    output            = std::copy(target_beg, target_end, output);
    remaining        -= copy_len;
    current_pos      += copy_len;

    // Literal
    lit_len           = std::min(lit_len, remaining);
    auto end_lit      = std::next(lit_it, lit_len);
    output            = std::copy(lit_it, end_lit, output);
    lit_it            = end_lit;
    remaining        -= lit_len;
    current_pos      += lit_len;

    // Next phrases
    std::tie(start_copy, ptr, copy_len) = *parse_it++;
    lit_len                 = *len_it++;
    copy_len               -= lit_len;
  }
}

template <typename Alphabet, typename Source, typename ParseKeeper, typename LiteralKeeper>
typename Alphabet::Symbol index<Alphabet, Source, ParseKeeper, LiteralKeeper>::operator()(size_t idx) const
{
  // Find phrase and subphrase
  size_t phrase, subphrase;
  std::tie(phrase, subphrase) = parse.phrase_subphrase(idx);

  auto next_phrase = parse.start_subphrase(subphrase + 1);
  auto junk_length = literals.literal_length(subphrase);
  const auto junk_start  = next_phrase - junk_length;
  if (junk_start <= idx) {
    return *std::next(literals.literal_access(subphrase), idx - junk_start);
  } else {
    auto ptr = parse.get_ptr(phrase, subphrase);
    assert((ptr >= 0) or (-ptr < idx));
    return source[get_target(idx, ptr)];
  }
}

template <typename Alphabet, typename Source, typename ParseKeeper, typename LiteralKeeper>
std::vector<typename Alphabet::Symbol> index<Alphabet, Source, ParseKeeper, LiteralKeeper>::operator()(size_t begin, size_t end) const
{
  std::vector<Symbol> to_ret(end - begin);
  (*this)(begin, end, to_ret.begin());
  return to_ret;
}

}
