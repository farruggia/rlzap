#pragma once

#include "containers.hpp"
#include "impl/get_matchings.hpp"
#include "match.hpp"
#include "sa_compute.hpp"

#include <algorithm>
#include <chrono>
#include <istream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <sdsl/int_vector.hpp>

namespace rlz {

/*
 * Returns:
 * - List of matches (one for each input position),
 * - Content of reference,
 * - Content of input
 */
template <typename Alphabet, typename RefDump, typename InDump>
std::tuple<
  std::vector<match>,
  mapped_stream<Alphabet>,
  mapped_stream<Alphabet> 
> get_relative_matches(
  RefDump &ref_dump, InDump &in_dump
)
{
  using Symbol = typename Alphabet::Symbol;
  namespace chr = std::chrono;
  // auto t_1 = chr::high_resolution_clock::now();

  /* Get joined string */
  auto Tsh = std::make_shared<std::vector<Symbol>>();
  auto &T  = *Tsh;
  size_t ref_len, input_len;
  std::tie(input_len, ref_len) = impl::read_joined<Alphabet>(in_dump, ref_dump, T);

  // auto t_2 = chr::high_resolution_clock::now();
  // std::cerr << "--- Reading  = " 
  //           << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
  //           << " μs" << std::endl;

  /* Compute SA and LCP values */
  sdsl::int_vector<> SA, LCP;

#if 0
  for (auto i : T) {
    std::cout << std::setw(2) << i << " ";
  }
  std::cout << "\n";
  for (auto i = 0U; i < T.size(); ++i) {
    std::cout << std::setw(2) << i << " ";
  }
  std::cout << std::endl;
#endif

  // t_1 = chr::high_resolution_clock::now();
  rlz::sa_compute<Symbol>{}(T.data(), SA, LCP, T.size());
  // t_2 = chr::high_resolution_clock::now();
  // std::cerr << "--- SA       = " 
  //           << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
  //           << " μs" << std::endl;


#if 0
  std::cout << "Alphabet symbol: " << typeid(Symbol).name() << std::endl;
  // std::cout << "  REF SA LCP DLCP SUF" << std::endl;
  std::cout << "  REF SA LCP DLCP" << std::endl;
  int prev = 0;
  for (auto i = 0U; i < SA.size(); ++i) {
    auto sa_v  = SA[i];
    auto lcp_v = LCP[i];

    std::stringstream ss;
    if (sa_v < input_len) {
      ss << "I(" << std::setw(2) << sa_v << ")";
    } else if (sa_v > input_len) {
      ss << "R(" << std::setw(2) << (sa_v - input_len - 1) << ")";
    } else {
      ss << "-SEP-";
    }

    auto T_v = std::vector<Symbol>(T.data() + sa_v, T.data() + T.size());
    std::cout << std::setw(5) << ss.str() << " "
              << std::setw(3) << sa_v << " "
              << std::setw(3) << lcp_v << " "
              << std::setw(4) << static_cast<int>(lcp_v) - prev << " ";
    // for (auto v : T_v) {
    //   std::cout << std::setw(3) << v << " ";
    // }
    std::cout << std::endl;
    prev = lcp_v;
  }
#endif

  T[input_len] = Symbol{};

  // t_1 = chr::high_resolution_clock::now();
  auto M = impl::longest_match(SA.begin(), LCP.begin(), ref_len, input_len);
  // t_2 = chr::high_resolution_clock::now();
  // std::cerr << "--- Matching = " 
  //           << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
  //           << " μs" << std::endl;

  mapped_stream<Alphabet> input_s(Tsh, 0U, input_len);
  mapped_stream<Alphabet> ref_s(Tsh, input_len + 1, ref_len);
  return std::make_tuple(std::move(M), ref_s, input_s);
}

}