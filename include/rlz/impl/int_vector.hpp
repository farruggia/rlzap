#pragma once

#include <cstdint>
#include <array>

namespace rlz {
namespace ds {
namespace impl {
  
template <size_t Width>
struct unsign {
  static constexpr std::uint64_t mask = ((1ULL << Width) - 1);
  
  std::uint64_t operator()(const std::int64_t &value)
  {
    return value & mask;
  }
};

template <size_t Width>
struct sign {
  static constexpr size_t NWidth = Width - 1;
  static constexpr std::uint64_t neg_mask = 0xFFFFFFFFFFFFFFFF - ((1ULL << Width) - 1);
  static std::array<std::uint64_t, 2> masks;

  std::int64_t operator()(const std::uint64_t &value) const
  {
    bool leading = value >> NWidth;
    return value | masks[leading];
  }
};

template <size_t Width>
std::array<std::uint64_t, 2> sign<Width>::masks {{ 0, sign<Width>::neg_mask }};

template<typename T>
struct cast_unsign {
  std::uint64_t operator()(const std::int64_t &value)
  {
    return static_cast<T>(value);
  }
};

template<typename T>
struct cast_sign {
  std::int64_t operator()(const std::uint64_t &value)
  {
    return static_cast<T>(value);
  }
};

template <> struct unsign<32> : public cast_unsign<std::uint32_t> { };
template <> struct sign<32>   : public cast_sign<std::int32_t> { };

template <> struct unsign<16> : public cast_unsign<std::uint16_t> { };
template <> struct sign<16>   : public cast_sign<std::int16_t> { };

template <> struct unsign<8>  : public cast_unsign<std::uint8_t> { };
template <> struct sign<8>    : public cast_sign<std::int8_t> { };

}
}
}
