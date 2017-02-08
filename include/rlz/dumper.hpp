#pragma once

#include <array>
#include <istream>
#include <vector>

namespace rlz { namespace utils {

template <typename It>
class iterator_dumper {
  It begin;
  It end;
public:
  iterator_dumper (It begin, It end) : begin(begin), end(end) { }

  template <typename T>
  void dump(std::vector<T> &v)
  {
    std::copy(begin, end, std::back_inserter(v));
  }
};

template <typename It>
iterator_dumper<It> get_iterator_dumper(It begin, It end)
{
  return iterator_dumper<It>(begin, end);
}

class stream_dumper {
  std::istream &s;
public:
  stream_dumper (std::istream &s) : s(s) { }

  template <typename T>
  void dump(std::vector<T> &v)
  {
    std::array<T, 1024> buffer;
    char *buf     = reinterpret_cast<char*>(buffer.data());
    size_t buf_size = buffer.size() * sizeof(T);
    while (s) {
      s.read(buf, buf_size);
      auto read = s.gcount() / sizeof(T);
      std::copy(buffer.begin(), buffer.begin() + read, std::back_inserter(v));
    }
  }
};

}}