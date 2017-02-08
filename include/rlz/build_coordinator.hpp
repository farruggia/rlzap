#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace rlz {
namespace build {

template <typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class agnostic_observer {
public:
  // returns (max copy, max literal) which is OK for this component.
  virtual std::tuple<std::size_t, std::size_t> can_split(
    std::size_t phrase_start, std::ptrdiff_t copy_delta, std::size_t copy_len, std::size_t junk_len
  ) = 0;

  // Returns true if splitting here allocates an another block.
  virtual bool split_as_block(
    std::size_t phrase_start, std::ptrdiff_t copy_delta, std::size_t copy_len, std::size_t junk_len
  ) = 0;

  // Splits here
  virtual void split(
    std::size_t phrase_start, std::ptrdiff_t copy_delta, std::size_t copy_len, std::size_t junk_len,
    const SymbolIt bytes, bool block_split
  ) = 0;

  // Parsing finished
  virtual void finish() = 0;
};

template <typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class observer {
public:
  // returns maximum prefix length which is OK to split.
  virtual size_t can_split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt bytes) = 0;

  // Returns true if splitting here allocates an another block.
  virtual bool split_as_block(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt bytes) = 0;

  // Splits at given phrase.
  virtual void split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt bytes, bool block_split) = 0;

  // Parsing finished
  virtual void finish() = 0;
};

template <typename Agnostic, typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class forward_observer_adapter : public observer<Alphabet, SymbolIt> {
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
  size_t can_split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt) override
  {
    std::size_t copy, lit;
    auto wrapped = this->get_wrapped();
    auto delta = this->delta_get(source, target);
    std::tie(copy, lit) = wrapped->can_split(source, delta, len, junk_len);
    return copy + lit;
  }

  // Returns true if splitting here allocates an another block.
  bool split_as_block(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt) override
  {
    auto wrapped = this->get_wrapped();
    auto delta = this->delta_get(source, target);
    return wrapped->split_as_block(source, delta, len, junk_len);
  }

  // Splits at given phrase.
  void split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt bytes, bool block_split) override
  {
    auto wrapped = this->get_wrapped();
    auto delta = this->delta_get(source, target);
    wrapped->split(source, delta, len, junk_len, bytes, block_split);
  }

  // Parsing finished
  void finish() override
  {
    this->get_wrapped()->finish();
  }
};

template <typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class coordinator {
private:
  using Symbol = typename Alphabet::Symbol;

  struct copy {
    size_t source;
    size_t target;
    size_t length;

    copy(size_t source, size_t target, size_t length) 
      : source(source), target(target), length(length) { }
    copy() : copy(0U, 0U, 0U) { }
  };
  std::vector<std::shared_ptr<observer<Alphabet, SymbolIt>>> components;
  const SymbolIt text;
  std::tuple<bool, copy> previous;
  size_t committed_position;

  bool previous_is_copy()
  {
    return std::get<0>(previous);
  }

  void set_previous_copy(size_t source, size_t target, size_t length)
  {
    previous = std::make_tuple(true, copy(source, target, length));
  }

  void set_previous_literal()
  {
    previous = std::make_tuple(false, copy());    
  }

  copy get_previous_copy()
  {
    return std::get<1>(previous);
  }

  void commit(size_t source, size_t target, size_t copy_length, size_t literal)
  {
    // Get minimum length
    auto literal_start  = text + committed_position + copy_length;
    size_t candidate    = copy_length + literal, original_phrase = candidate;
    for (auto &i : components) {
      candidate = std::min(
        candidate,
        i->can_split(source, target, copy_length, literal, literal_start)
      );
    }

    // Get phrase to be committed
    assert(candidate > 0U and candidate <= copy_length + literal);
    size_t commit_copy, commit_literal;
    if (candidate < copy_length) {
      commit_copy     = candidate;
      commit_literal  = 0U;
    } else {
      commit_copy     = copy_length;
      commit_literal  = candidate - commit_copy;
    }

    // New block?
    bool is_block = false;
    for (auto &i : components) {
      is_block = is_block or i->split_as_block(source, target, commit_copy, commit_literal, literal_start);
    }

    // Commit that phrase
    for (auto &i : components) {
      i->split(source, target, commit_copy, commit_literal, literal_start, is_block);
    }
    committed_position += commit_copy + commit_literal;

    // Handles the remaining part, if any
    if (candidate < original_phrase) {
      auto left_copy = copy_length - commit_copy;
      auto left_lit  = literal - commit_literal;
      commit(committed_position, target + commit_copy, left_copy, left_lit);
    }    
  }

public:
  template <typename It>
  coordinator(It components_begin, It components_end, const SymbolIt text)
    : components(components_begin, components_end), text(text), 
      previous(std::make_tuple(false, copy())),
      committed_position(0U)
  {

  }

  void copy_evt(size_t position, size_t ptr, size_t len)
  {
    if (previous_is_copy()) {
      auto cpy = get_previous_copy();
      commit(cpy.source, cpy.target, cpy.length, 0U);
    }
    set_previous_copy(position, ptr, len);
  }

  void literal_evt(size_t position, size_t length)
  {
    copy cpy(position, position, 0);
    if (previous_is_copy()) {
      cpy = get_previous_copy();
    }
    commit(cpy.source, cpy.target, cpy.length, length);
    set_previous_literal();
  }

  void end_evt()
  {
    if (previous_is_copy()) {
      auto cpy = get_previous_copy();
      commit(cpy.source, cpy.target, cpy.length, 0U);    
    }
    for (auto &i : components) {
      i->finish();
    }
  }

};
  
}  
}