#pragma once

#include <cstdint>
#include <cstdlib>

namespace rlz { namespace alphabet {

template <std::size_t bits>
struct bits_traits {
  static_assert(bits < 64UL, "Cannot instantiate integral alphabet of more than 64 bits");
  using Type = typename bits_traits<bits + 1>::Type;
};

template <> struct bits_traits<8UL>  { using Type = std::uint8_t;  };
template <> struct bits_traits<16UL> { using Type = std::uint16_t; };
template <> struct bits_traits<32UL> { using Type = std::uint32_t; };
template <> struct bits_traits<64UL> { using Type = std::uint64_t; };

}}