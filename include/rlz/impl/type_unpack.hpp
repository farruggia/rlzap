#pragma once

namespace rlz {
namespace impl {

template <typename...>
struct type_list {
};

template <template <typename...> class, typename>
struct compone {
};

template <template <typename...> class C, typename... Z>
struct compone<C, type_list<Z...>> {
  using type = C<Z...>;
};


template <template <typename...> class C, typename TypeList>
using Compone = typename compone<C, TypeList>::type;

}
}