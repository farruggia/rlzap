#pragma once

#include <cassert>
#include <cstdint>
#include <iterator>
#include <vector>

#include <boost/iterator/iterator_facade.hpp>

#include <sdsl/io.hpp>
#include <sdsl/util.hpp>

namespace rlz {

template <size_t bits>
class fractional_byte_container {
  static const size_t max_size          = (1UL << bits) - 1;
  static const size_t elements_per_byte = 8UL / bits;
  size_t elements;
  std::vector<std::uint8_t> storage;
  static_assert(bits <= 8UL and (8UL % bits) == 0, "bits must be a power of two and less than 8");

  size_t vec_size()
  {
    return (elements * bits + 7) / 8U;
  }

public:

  fractional_byte_container() { }

  template <typename It>
  fractional_byte_container(It begin, It end) 
    : elements(std::distance(begin, end)), storage(vec_size(), 0UL)
  {
    std::uint8_t *data = storage.data();
    auto i = 0U;
    for (auto it = begin; it != end; ++it) {
      if (i == elements_per_byte) {
        ++data;
        i = 0U;
      }
      assert(*it <= max_size);
      *data = *data | (*it << (i++ * bits));
    }
  }

  class iter;

  std::uint8_t operator[](size_t idx) const
  {
    return (storage[idx / elements_per_byte]  >> (bits * (idx % elements_per_byte))) & max_size;
  }

  std::uint8_t *data()
  {
    return storage.data();
  }

  const std::uint8_t *data() const
  {
    return storage.data();
  }

  size_t size() const
  {
    return elements;
  }

  iter begin() const
  {
    return iter(data(), 0UL);
  }

  iter end() const
  {
    return iter(data() + elements / elements_per_byte, bits * (elements % elements_per_byte));
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    size_t written_bytes = 0; // TODO: da scrivere
    written_bytes += sdsl::write_member(elements, out, v, name);
    written_bytes += sdsl::serialize_vector(storage, out, v, name);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    sdsl::read_member(elements, in);
    storage.resize(vec_size());
    sdsl::load_vector(storage, in);
  }

};

template <size_t bits>
class fractional_byte_container<bits>::iter
  : public boost::iterator_facade<
      typename fractional_byte_container<bits>::iter,
      std::uint8_t,
      std::random_access_iterator_tag,
      std::uint8_t
    >
{
  const static int elements_per_byte = 8UL / bits;
  const static int mask              = (1UL << bits) - 1;
  const std::uint8_t *ptr;
  size_t       offset;
public:
  iter() : ptr(nullptr), offset(0U) { }

  iter(const std::uint8_t *ptr, size_t offset) : ptr(ptr), offset(offset) { }

private:

  friend class boost::iterator_core_access;

  bool equal(iter const& other) const
  {
    return ptr == other.ptr and offset == other.offset;
  }

  int distance_to(iter const &other) const
  {
    return (other.ptr - ptr) * elements_per_byte + (static_cast<int>(other.offset) - static_cast<int>(offset)) / bits;
  }

  void increment()
  {
    offset += bits;
    if (offset == 8UL) {
      ++ptr;
      offset = 0U;
    }
  }

  void decrement()
  {
    if (offset == 0UL) {
      offset = 8UL;
      --ptr;
    }
    offset -= bits;
  }

  void advance(int n)
  {
    ptr    += n / elements_per_byte;
    offset += 8;
    offset += (n % elements_per_byte) * static_cast<int>(bits);
    ptr    += (static_cast<int>(offset) / 8) - 1;
    offset = offset % 8;
  }


  std::uint8_t dereference() const
  {
    return (*ptr >> offset) & mask;
  }
};

}