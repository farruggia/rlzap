#pragma once

#include <cstdlib>

namespace rlz {
namespace values {
template <int Value>
struct Int {
  constexpr static int value() { return Value; };
};

template <std::size_t Value>
struct Size {
  constexpr static std::size_t value() { return Value; };
};

template <int Num, int Den>
struct Rational {
  constexpr static double value() { return static_cast<double>(Num) / Den; };
};

template <bool Value>
struct Boolean {
  constexpr static bool value() { return Value; }
};

}
}