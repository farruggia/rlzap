#pragma once

#include <cstdint>
#include <iostream>
#include <tuple>

#include <sdsl/config.hpp>
#include <sdsl/structure_tree.hpp>

namespace rlz { namespace io { namespace sdsl_like {

template <typename T>
std::ostream &write_pod(T t, std::ostream &s)
{
  return s.write(reinterpret_cast<char*>(&t), sizeof(t));
}

template <typename T>
std::tuple<sdsl::int_vector_size_type, std::uint8_t> length_width(size_t n)
{
  std::uint8_t width = sizeof(T) * 8;
  sdsl::int_vector_size_type length = n * width;  
  return std::make_tuple(length, width);
}

template <typename T>
size_t serialize_fixed(const T *ptr, size_t n, std::ostream &s)
{
  // Write size
  auto length = std::get<0>(length_width<T>(n));
  write_pod(length, s);
  // Write data
  s.write(reinterpret_cast<const char*>(ptr), sizeof(*ptr) * n);
  return sizeof(length) + sizeof(*ptr) * n;
}

template <typename T>
size_t serialize_variable(const T *ptr, size_t n, std::ostream &s)
{
  // Get width and length (both in bits)
  std::uint8_t width;
  sdsl::int_vector_size_type length;
  std::tie(length, width) = length_width<T>(n);
  // Write fields
  write_pod(length, s);
  write_pod(width, s);
  // Write data
  s.write(reinterpret_cast<const char*>(ptr), sizeof(*ptr) * n);
  return sizeof(length) + sizeof(width) +  sizeof(*ptr) * n;
}

template <typename T>
struct variable_array {
  T *data;
  size_t n;

  using size_type = size_t;

  variable_array(T *data, size_t n) : data(data), n(n) { }

  variable_array() : variable_array(nullptr, 0UL) { }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=NULL, std::string name="") const
  {
    return serialize_variable(data, n, out);
  }
};

template <typename T>
struct fixed_array {
  T *data;
  size_t n;

  using size_type = size_t;

  fixed_array(T *data, size_t n) : data(data), n(n) { }

  fixed_array() : fixed_array(nullptr, 0UL) { }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=NULL, std::string name="") const
  {
    return serialize_fixed(data, n, out);
  }
};

} } }