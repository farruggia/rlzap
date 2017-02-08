#pragma once

#include <assert.h>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

#include "../build_coordinator.hpp"

namespace rlz { namespace lcp { namespace build {

template <typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class observer {
public:
  // returns maximum prefix length which is OK to split.
  virtual std::size_t can_split(
    std::size_t source,
    const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len
  ) = 0;

  // Returns true if splitting here allocates an another block.
  virtual bool split_as_block(
    std::size_t source,
    const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len
  ) = 0;

  // Splits at given phrase.
  virtual void split(
    std::size_t source,
    const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len,
    bool block_split
  ) = 0;

  // Parsing finished
  virtual void finish() = 0;

  virtual ~observer() { }
};

template <typename Agnostic, typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class lcp_observer_adapter : public observer<Alphabet, SymbolIt> {
private:
  std::ptrdiff_t delta_get(std::size_t source, std::size_t target)
  {
    std::ptrdiff_t s_source = source, s_target = target;
    return s_target - s_source;
  }
protected:
  virtual Agnostic *get_wrapped() = 0;
public:
  // returns maximum prefix length which is OK to split.

  std::size_t can_split(
    std::size_t source, const SymbolIt bytes, std::size_t junk_len,
    std::size_t target, std::size_t len
  ) override
  {
    std::size_t copy, lit;
    auto wrapped = this->get_wrapped();
    auto delta = this->delta_get(source + junk_len, target);
    std::tie(copy, lit) = wrapped->can_split(source, delta, len, junk_len);
    return lit < junk_len ? lit : copy + lit;
  }

  bool split_as_block(
    std::size_t source, const SymbolIt bytes, std::size_t junk_len, std::size_t target, std::size_t len
  ) override
  {
    auto wrapped = this->get_wrapped();
    auto delta = this->delta_get(source + junk_len, target);
    return wrapped->split_as_block(source, delta, len, junk_len);    
  }

  void split(
    std::size_t source, const SymbolIt bytes, std::size_t junk_len, std::size_t target, std::size_t len,
    bool block_split
  ) override
  {
    auto wrapped = this->get_wrapped();
    auto delta = this->delta_get(source + junk_len, target);
    wrapped->split(source, delta, len, junk_len, bytes, block_split);
  }
  
  // Parsing finished
  void finish() override
  {
    this->get_wrapped()->finish();
  }
};

template <
  typename Alphabet,
  typename SymbolIt = typename Alphabet::Symbol*, typename RefIt = typename Alphabet::Symbol*
>
class coordinator {
private:
  using Symbol = typename Alphabet::Symbol;

  struct literal {
    std::size_t source;
    std::size_t length;

    literal(std::size_t source, std::size_t length) 
      : source(source), length(length) { }
    literal() : literal(0U, 0U) { }
  };
  std::vector<std::shared_ptr<observer<Alphabet, SymbolIt>>> components;
  SymbolIt text;
  RefIt    reference;
  std::tuple<bool, literal> previous;
  std::size_t committed_position;

  bool previous_is_literal()
  {
    return std::get<0>(previous);
  }

  void set_previous_literal(std::size_t source, std::size_t length)
  {
    previous = std::make_tuple(true, literal(source, length));
  }

  void set_previous_copy()
  {
    previous = std::make_tuple(false, literal());    
  }

  literal get_previous_literal()
  {
    return std::get<1>(previous);
  }

  void commit(std::size_t source_start, std::size_t literal_len, std::size_t target, std::size_t copy_length)
  {
    // Get minimum length
    auto literal_start = std::next(text, source_start);
    std::size_t candidate = copy_length + literal_len, original_phrase = candidate;
    for (auto &i : components) {
      candidate = std::min(
        candidate,
        i->can_split(source_start, literal_start, literal_len, target, copy_length)
      );
    }

    // Get phrase to be committed
    assert(candidate > 0U and candidate <= original_phrase);
    std::size_t commit_copy, commit_literal;
    if (candidate < literal_len) {
      commit_literal = candidate;
      commit_copy    = 0U;
    } else {
      commit_literal  = literal_len;
      commit_copy     = candidate - commit_literal;
    }

    // New block?
    bool is_block = false;
    for (auto &i : components) {
      is_block = is_block or i->split_as_block(
        source_start, literal_start, commit_literal,
        target, commit_copy
      );
    }

    // Commit that phrase
    for (auto &i : components) {
      i->split(
        source_start, literal_start, commit_literal,
        target, commit_copy,
        is_block
      );
    }
    committed_position += commit_copy + commit_literal;

    // Handles the remaining part, if any
    if (candidate < original_phrase) {
      // Chopping literals or copy-lengths?
      if (literal_len > commit_literal) {
        auto left_lit  = literal_len - commit_literal;
        commit(committed_position, left_lit, target, copy_length);
      } else {
        // Should we introduce a "compulsory literal" here?
        std::ptrdiff_t delta = target - (source_start + literal_len);
        auto source_sym = *std::next(text, committed_position);
        auto ref_sym    = *std::next(reference, committed_position + delta);
        auto left_copy = copy_length - commit_copy;
        if (source_sym == ref_sym) { // Nope
          commit(committed_position, 0UL, target + commit_copy, left_copy);
        } else { // Yes
          commit(committed_position, 1UL, target + commit_copy + 1, left_copy - 1);
        }
      }
    }
  }

public:

  coordinator() { }

  template <typename It>
  coordinator(It components_begin, It components_end, const SymbolIt text, const RefIt reference)
    : components(components_begin, components_end), text(text), reference(reference),
      previous(std::make_tuple(false, literal())),
      committed_position(0U)
  {

  }

  void copy_evt(std::size_t position, std::size_t ptr, std::size_t len)
  {
    std::size_t source = position, lit_len = 0UL;
    if (previous_is_literal()) {
      auto cpy = get_previous_literal();
      source  = cpy.source;
      lit_len = cpy.length;
    }
    commit(source, lit_len, ptr, len);
    set_previous_copy();
  }

  void literal_evt(std::size_t position, std::size_t length)
  {
    if (previous_is_literal()) {
      auto lit = get_previous_literal();
      commit(lit.source, lit.length, 0UL, 0UL);
    }
    set_previous_literal(position, length);
  }

  void end_evt()
  {
    if (previous_is_literal()) {
      auto lit = get_previous_literal();
      commit(lit.source, lit.length, 0U, 0U);    
    }
    for (auto &i : components) {
      i->finish();
    }
  }
};

}}}