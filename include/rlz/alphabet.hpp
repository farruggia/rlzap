#pragma once

#include <cstdint>
#include <type_traits>

#include "dna_packer.hpp"
#include "lcp_pack.hpp"
#include "impl/alphabet.hpp"
#include "integer_pack.hpp"
#include "integer_type.hpp"

namespace rlz { namespace alphabet {

template <typename BlockSize = rlz::values::Size<8UL>>
struct dna {
  using Symbol      = char;
  using Packer      = dna_pack<BlockSize>;
};

template <typename Bits>
struct integer {
  using Symbol      = typename bits_traits<Bits::value()>::Type;
  using Packer      = integer_pack<Bits>;
};

template <size_t B>
using Integer = integer<rlz::values::Size<B>>;

template <typename T>
struct lcp {
  using Symbol      = T;
  using Packer      = lcp_pack<T>;
};

using dlcp_16 = lcp<std::int16_t>;
using dlcp_32 = lcp<std::int32_t>;
using dlcp_64 = lcp<std::int64_t>;
using lcp_16  = lcp<std::uint16_t>;
using lcp_32  = lcp<std::uint32_t>;
using lcp_64  = lcp<std::uint64_t>;

namespace detail {

  template <typename T>
  struct sign_lcp {};

  template <typename T>
  struct sign_lcp<lcp<T>> {
    using sign_t = lcp<typename std::make_signed<T>::type>;
    using unsign_t = lcp<typename std::make_unsigned<T>::type>;
  };
}

template <typename T>
using SignLcp = typename detail::sign_lcp<T>::sign_t;

template <typename T>
using UnsignLcp = typename detail::sign_lcp<T>::unsign_t;

} }
