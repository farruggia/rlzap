#pragma once

#include <cstddef>
#include <cstdint>

// Generates a static table from a metafunction, at compile time, using variadic templates.
// Taken from http://stackoverflow.com/a/2981617/69178

namespace rlz { namespace static_table {

template<std::uint8_t... args> struct ArrayHolder {
    static const std::uint8_t data[sizeof...(args)];
};

template<std::uint8_t... args> 
const std::uint8_t ArrayHolder<args...>::data[sizeof...(args)] = { args... };

template<size_t N, template<size_t> class F, std::uint8_t... args> 
struct generate_array_impl {
    using result = typename generate_array_impl<N-1, F, F<N>::value, args...>::result ;
};

template<template<size_t> class F, std::uint8_t... args> 
struct generate_array_impl<0, F, args...> {
    using result = ArrayHolder<F<0>::value, args...>;
};

template<size_t N, template<size_t> class F> 
struct generate_array {
    using result = typename generate_array_impl<N-1, F>::result;
};

}}