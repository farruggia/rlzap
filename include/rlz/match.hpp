#pragma once

#include <cstdint>
#include <iostream>
#include <limits>

namespace rlz {
struct match {
  size_t ptr;
  size_t len;

  match(size_t ptr, size_t len) : ptr(ptr), len(len) { }

  match() : match(std::numeric_limits<size_t>::max(), 0U)
  { }

  bool matched() { return len > 0U; }

  void store(std::ostream &os) const
  {
    static_assert(sizeof(std::uint64_t) >= sizeof(size_t), "size_t don't fit into a uint64_t");
    std::uint64_t c_ptr = ptr, c_len = len;
    os.write(reinterpret_cast<char*>(&c_ptr), sizeof(c_ptr));
    os.write(reinterpret_cast<char*>(&c_len), sizeof(c_len));
  }

  static match load(std::istream &is)
  {
    std::uint64_t ptr, len;
    is.read(reinterpret_cast<char*>(&ptr), sizeof(ptr));
    is.read(reinterpret_cast<char*>(&len), sizeof(len));
    return match {ptr, len};
  }
};
}