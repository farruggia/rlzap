#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <sstream>
#include <type_traits>

#include <sdsl/construct_lcp.hpp>
#include <sdsl/construct_sa.hpp>
#include <sdsl/int_vector.hpp>
#include <sdsl/io.hpp>

#include "cache_settings.hpp"
#include "sdsl_extensions/serialize.hpp"


namespace rlz {

namespace impl {

  enum class TextKind {
    TEXT,
    INT
  };

  template <typename T>
  struct sa_compute_traits {
    static const size_t width  = 0UL;
    static const TextKind kind = TextKind::INT;
    using wrapper_type         = io::sdsl_like::variable_array<T>;
  };

  template <>
  struct sa_compute_traits<char> {
    static const size_t width  = 8UL;
    static const TextKind kind = TextKind::TEXT;
    using wrapper_type         = io::sdsl_like::fixed_array<char>;
  };

}

template <typename T>
class sa_compute {
private:
    static const impl::TextKind kind = impl::sa_compute_traits<T>::kind;
    static const size_t width        = impl::sa_compute_traits<T>::width;
    using wrapper_t                  = typename impl::sa_compute_traits<T>::wrapper_type;

    const char *get_key_name(impl::TextKind kind) const
    {
      switch (kind) {
        case impl::TextKind::TEXT: return sdsl::conf::KEY_TEXT;
        case impl::TextKind::INT:  return sdsl::conf::KEY_TEXT_INT;
        default:                   throw std::logic_error("Unknown text kind in sa_compute");
      }
    }

    std::string get_random_id() const
    {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dis(0, std::numeric_limits<unsigned int>::max());
      std::stringstream ss;
      ss << dis(gen) << dis(gen);
      return ss.str();
    }

public:

  sa_compute() { }

  void operator()(T *data, sdsl::int_vector<> &sa, sdsl::int_vector<> &lcp, std::size_t length) const
  {
    T displacement;
    auto increment = [&] () {
      for (std::size_t i = 0U; i < length - 1; ++i) {
        assert(data[i] < std::numeric_limits<T>::max());
        data[i] += displacement;
      }
    };
    auto decrement = [&] () {
      for (std::size_t i = 0U; i < length - 1; ++i) {
        assert(data[i] > std::numeric_limits<T>::lowest());
        data[i] -= displacement;
      }
    };

    T min = std::numeric_limits<T>::max(), max = std::numeric_limits<T>::min();
    for (auto it = data; it < data + length; ++it) {
      min = std::min(min, *it);
      max = std::max(max, *it);
    }
    if (min < static_cast<T>(1)) {
      if (max > std::numeric_limits<T>::max() + min - 1) {
        throw std::logic_error("SA computation: cannot transform input range to exclude null elements");
      }
      displacement = 1 - min;
    } else {
      displacement = T{};
    }

    auto key   = get_key_name(kind);
    auto f_id  = get_random_id();
    sdsl::cache_config cache {true, cache::global_settings::temp_directory, f_id.c_str()};
    increment();
    // Store data on disk
    {
      wrapper_t serialize_data(data, length);
      sdsl::store_to_cache(serialize_data, key, cache);
    }
    // Compute SA
    sdsl::construct_sa<width>(cache);
    // Compute LCP
    sdsl::construct_lcp_PHI<width>(cache);
    // Retrieve SA & LCP
    if (!sdsl::load_from_cache(sa, sdsl::conf::KEY_SA, cache)) {
      throw std::logic_error("Cannot recover SA from cache");
    }
    if (!sdsl::load_from_cache(lcp, sdsl::conf::KEY_LCP, cache)) {
      throw std::logic_error("Cannot recover LCP from cache");
    }
    decrement();

    // Clear files
    auto delete_key = [&cache] (const char *key) {
      auto file_name = cache_file_name(key, cache);
      if (std::remove(file_name.c_str())) {
        std::cerr << "ERROR: cannot remove file " << file_name << std::endl;
      }
    };
    delete_key(key);
    delete_key(sdsl::conf::KEY_SA);
    // delete_key(sdsl::conf::KEY_ISA);
    delete_key(sdsl::conf::KEY_LCP);
  }
};

}
