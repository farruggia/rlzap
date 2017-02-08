#pragma once

#include <type_traits>

namespace rlz {
namespace type_utils {

// Remove ALL qualifiers
template <typename T>
using RemoveQualifiers = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

// Check if something is T or is a proxy object which can be converted *exactly* to T
template <typename T, typename U>
constexpr auto convertible(int) -> typename std::enable_if<sizeof(decltype(T{}.operator U())) == sizeof(U), bool>::type
{
  return true;
}

template <typename T, typename U>
constexpr bool convertible(double)
{
  return false;
}

template <typename T, typename U>
constexpr bool is_complaint()
{
  using Z = RemoveQualifiers<T>;
  return std::is_same<Z, U>::value or convertible<T, U>(0);
}


template <typename...T>
struct type_list { };

// Apply
template <template <typename...> class T, typename V>
struct Apply { };

template <template <typename...> class T, typename... Params>
struct Apply<T, type_list<Params...>> {
  using type = T<Params...>;
};

template <template <typename...> class T, typename TL, typename... TList>   // TList: list of list of types. list of T<list of types>
struct IInst {
};

template <template <typename...> class T, typename... Partial, typename U, typename... TList>
struct IInst<T, type_list<Partial...>, U, TList...> {
  using type = typename IInst<T, type_list<Partial..., typename Apply<T, U>::type>, TList...>::type;
};

template <template <typename...> class T, typename... Partial>
struct IInst<T, type_list<Partial...>> {
  using type = type_list<Partial...>;
};

// Inst
template <template <typename...> class T, typename TList>   // TList: list of list of types. list of T<list of types>
struct Inst { };

template <template <typename...> class T, typename... TList>    // TList: list of list of types. list of T<list of types>
struct Inst<T, type_list<TList...>> {
  using type = typename IInst<T, type_list<>, TList...>::type;
};

// Wrap
template<typename Partial, typename Remaining>
struct Wrap { };

template<typename... PList, typename RHead, typename... RTail>
struct Wrap<type_list<PList...>, type_list<RHead, RTail...>> {
  using type = typename Wrap<type_list<PList..., type_list<RHead>>, type_list<RTail...>>::type;
};

template<typename... PList>
struct Wrap<type_list<PList...>, type_list<>> {
  using type = type_list<PList...>;
};

template <template <typename...> class T, typename TList>
struct WInst { };

template <template <typename...> class T, typename... TList>    // TList: list of list of types. list of T<list of types>
struct WInst<T, type_list<TList...>> {
  using type = typename Inst<T, typename Wrap<type_list<>, type_list<TList...>>::type>::type;
};

// SProd
template <typename List, typename T>
struct SUnion { };

template<typename T, typename... List>
struct SUnion<type_list<List...>, T> {
  using type = type_list<List..., T>;
};

template <typename T, typename L_1, typename L_2>
struct IUnion { };

template<typename T, typename... Partial, typename LHead, typename... LTail>
struct IUnion<T, type_list<Partial...>, type_list<LHead, LTail...>> {
  using type = typename IUnion<T, type_list<Partial..., typename SUnion<LHead, T>::type>, type_list<LTail...>>::type;
};

template<typename T, typename... Partial>
struct IUnion<T, type_list<Partial...>, type_list<>> {
  using type = type_list<Partial...>;
};

template<typename T, typename List>
struct Union {
};

template <typename T, typename... List>
struct Union<T, type_list<List...>>
{
  using type = typename IUnion<T, type_list<>, type_list<List...>>::type;
};

template <typename T>
struct Union<T, type_list<>>
{
  using type = type_list<type_list<T>>;
};

template<typename A, typename...B>
struct join { };

template<typename... L, typename... C>
struct join<type_list<L...>, C...> {
  using type = type_list<C..., L...>;
};

template <typename... Lists>
struct join_lists { };

template <typename List>
struct join_lists<List>
{
  using type = List;
};

template<typename... F, typename... S, typename ...R>
struct join_lists<type_list<F...>, type_list<S...>, R...> {
  using type = typename join_lists<type_list<F..., S...>, R...>::type;
};

template<typename L1, typename L2, typename L3> // L1 is the left cartesian set, L3 the right, L2 partial results.
struct ISProd { };

template<typename... L1, typename... L2, typename L3H, typename... L3T>
struct ISProd<type_list<L1...>, type_list<L2...>, type_list<L3H, L3T...>> {
  using type = typename ISProd<
            type_list<L1...>,                     // Still same list we must recurse over
            typename join<typename Union<L3H, type_list<L1...>>::type, L2...>::type,
            type_list<L3T...>                   // Recurse on the rest of the list
          >::type;
};

template<typename... L1, typename... L2>
struct ISProd<type_list<L1...>, type_list<L2...>, type_list<>> {
  using type = type_list<L2...>;
};

template <typename L1, typename L2>
struct SProd {
  using type = typename ISProd<L1, type_list<>, L2>::type;
};

template <typename...>
struct EProd { };

template<typename... TList, typename L_1, typename... Lists>
struct EProd<type_list<TList...>, L_1, Lists...> {
  using type = typename 
                EProd<
                  typename SProd<type_list<TList...>, L_1>::type, 
                  Lists...
                >::type;
};

template<typename... TList>
struct EProd<type_list<TList...>> {
  using type = type_list<TList...>;
};

template <template <typename...> class T, typename...Lists>
struct Prod {
  using type = typename Inst<T, typename EProd<type_list<>, Lists...>::type>::type;
};

}
}