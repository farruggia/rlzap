#pragma once

#include "match.hpp"

#include <cstdint>
#include <iostream>
#include <iterator>

namespace rlz { namespace serialize { namespace matches {

template <typename It>
void store(std::ostream &os, It begin, It end)
{
  std::uint64_t size = std::distance(begin, end);
  os.write(reinterpret_cast<char*>(&size), sizeof(size));
  for (auto it = begin; it != end; ++it) {
    it->store(os);
  }
}

template <typename Stream>
std::vector<match> load(Stream &s)
{
  std::uint64_t size;
  s.read(reinterpret_cast<char*>(&size), sizeof(size));
  std::vector<match> to_ret;
  to_ret.reserve(size);
  for (auto i = 0U; i < size; ++i) {
    to_ret.push_back(match::load(s));
  }
  return to_ret;
}

}}}