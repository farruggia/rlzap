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
  auto t_1 = chr::high_resolution_clock::now();

  /* Get joined string */
  auto Tsh = std::make_shared<std::vector<Symbol>>();
  auto &T  = *Tsh;
  size_t ref_len, input_len;
  std::tie(input_len, ref_len) = impl::read_joined<Alphabet>(in_dump, ref_dump, T);

  auto t_2 = chr::high_resolution_clock::now();
  std::cerr << "--- Reading  = " 
            << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
            << " μs" << std::endl;

  /* Compute SA and LCP values */
  sdsl::int_vector<> SA, LCP;

  t_1 = chr::high_resolution_clock::now();
  rlz::sa_compute<Symbol>{}(T.data(), SA, LCP, T.size());
  t_2 = chr::high_resolution_clock::now();
  std::cerr << "--- SA       = " 
            << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
            << " μs" << std::endl;


  T[input_len] = '\0';

  t_1 = chr::high_resolution_clock::now();
  auto M = impl::longest_match(SA.begin(), LCP.begin(), ref_len, input_len);
  t_2 = chr::high_resolution_clock::now();
  std::cerr << "--- Matching = " 
            << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
            << " μs" << std::endl;

  mapped_stream<Alphabet> input_s(Tsh, 0U, input_len);
  mapped_stream<Alphabet> ref_s(Tsh, input_len + 1, ref_len);
  return std::make_tuple(std::move(M), ref_s, input_s);
}

}