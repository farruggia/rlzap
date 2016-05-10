#pragma once

#include <sstream>

template <typename T>
T load_unload(const T &t)
{
  std::stringstream ss;
  t.serialize(ss);
  T new_t;
  new_t.load(ss);
  return new_t;
}