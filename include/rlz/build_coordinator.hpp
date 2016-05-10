#pragma once

#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

namespace rlz {
namespace build {

template <typename Alphabet>
class observer {
  using Symbol = typename Alphabet::Symbol;
public:
  // returns maximum prefix length which is OK to split.
  virtual size_t can_split(size_t source, size_t target, size_t len, size_t junk_len, const Symbol *bytes) = 0;

  // Returns true if splitting here allocates an another block.
  virtual bool split_as_block(size_t source, size_t target, size_t len, size_t junk_len, const Symbol *bytes) = 0;

  // Splits at given phrase.
  virtual void split(size_t source, size_t target, size_t len, size_t junk_len, const Symbol *bytes, bool block_split) = 0;

  // Parsing finished
  virtual void finish() = 0;
};

template <typename Alphabet>
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
  std::vector<std::shared_ptr<observer<Alphabet>>> components;
  const Symbol *text;
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
  coordinator(It components_begin, It components_end, const Symbol *text)
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