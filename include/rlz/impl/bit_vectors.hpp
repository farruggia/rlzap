#pragma once

#include <type_traits>
#include "../type_utils.hpp"

namespace rlz {
namespace impl {

template <typename ToTest, typename Expected>
using if_same = typename std::enable_if<
    std::is_same<
      type_utils::RemoveQualifiers<ToTest>,
      Expected
    >::value, 
    ToTest
  >::type;

}
}