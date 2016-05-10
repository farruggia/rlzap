#pragma once

#include <cstdint>

namespace rlz {
namespace impl {

template <size_t Width>
struct field_limits {
  static constexpr std::int64_t high()
  {
    return (1ULL << (Width - 1)) - 1;
  }

  static constexpr std::int64_t low()
  {
    return (-1 * high()) - 1;
  }
};
}
}