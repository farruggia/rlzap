#pragma once

#include <iterator>
#include <type_traits>

#include <boost/iterator/detail/facade_iterator_category.hpp>

namespace rlz {
namespace utils {

template <typename T>
struct is_random {
  static constexpr bool value = std::is_convertible<
    typename std::iterator_traits<T>::iterator_category, 
    std::random_access_iterator_tag
  >::value;

  constexpr operator bool() const { return value; }
};

}
}

#define CHECK_TYPE_RANDOM(T, MSG) static_assert(rlz::utils::is_random<T>::value, MSG)
#define CHECK_RANDOM(T, MSG) CHECK_TYPE_RANDOM(decltype(T), MSG)
