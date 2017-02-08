#pragma once

#include <cstddef>

namespace rlz { namespace prefix { namespace impl { 

  namespace table_gen {

  template <size_t bits, size_t el, size_t idx>
  struct get_val {
    enum { value = (el & ((1 << bits) - 1)) + get_val<bits, (el >> bits), idx - 1>::value };
  };

  template <size_t bits, size_t el>
  struct get_val<bits, el, 0>
  {
    enum { value = 0UL };
  };

  // i -> value. i: 8-bit
  template<size_t bits, size_t el>
  struct generator {
      enum { value = get_val<bits, el, (8UL / bits)>::value };
  };

  template <size_t bits>
  struct Generators {
    template <size_t element>
    using gen = generator<bits, element>;
  };
}

namespace mask_gen {

  template<size_t bits, size_t idx>
  struct generator {
      enum { value = ((1UL << ((idx + 1) * bits)) - 1) };
  };

  template <size_t bits>
  struct Generators {
    template <size_t element>
    using gen = generator<bits, element>;
  };
} 
}}}