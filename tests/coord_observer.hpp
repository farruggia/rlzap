#pragma once

#include <memory>
#include <sstream>
#include <build_coordinator.hpp>

template <typename Alphabet, typename SymbolIt = typename Alphabet::Symbol*>
class observer : public rlz::build::observer<Alphabet, SymbolIt> {
private:
  bool not_first_block;
  std::shared_ptr<std::stringstream> ss;
public:
  observer(std::shared_ptr<std::stringstream> ss) 
    : not_first_block(false), ss(ss) { }
  // returns maximum prefix length which is OK to split.
  size_t can_split(size_t, size_t, size_t len, size_t junk_len, const SymbolIt) override
  {
    return len + junk_len;
  }

  bool split_as_block(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt) override
  {
    return false;
  }

  // Splits at given phrase. Returns true if it must be the begin of a new block.
  void split(size_t source, size_t target, size_t len, size_t junk_len, const SymbolIt, bool block) override
  {
    if (block) {
      if (not_first_block) {
        (*ss) << "}";
      }
      not_first_block = true;
      (*ss) << "{";
    }
    (*ss) << "[";
    if (len > 0) {
      (*ss) << "C(" << source << "," << target << "," << len << ")";
    }
    if (junk_len > 0) {
      (*ss) << "L(" << junk_len << ")";
    }
    (*ss) << "]";
    block = false;
  }

  // Parsing finished
  void finish() override
  {
    (*ss) << "}.";
  }
};