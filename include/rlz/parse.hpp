#pragma once

#include "dumper.hpp"
#include "io.hpp"
#include "get_matchings.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <tuple>

namespace rlz {

class Parser {
private:
    const size_t edit_distance    = 30U;
    const size_t phrase_threshold = 15U;

public:

  Parser() { }

  Parser(size_t edit_distance, size_t phrase_threshold)
    : edit_distance(edit_distance), phrase_threshold(phrase_threshold)
  { }
    
  template <
    typename MatchIt, typename LiteralEvt, typename CopyEvt, 
    typename EndEvt = std::function<void()>
  >
  void operator() (
    MatchIt match_begin, MatchIt match_end, 
    size_t ref_len,
    LiteralEvt literal_func, CopyEvt copy_func, EndEvt end_evt
  ) const
  {
    const size_t E_L  = edit_distance;
    const size_t P_T  = phrase_threshold;
    auto m_it         = match_begin;
    auto target_pos   = m_it->ptr;

    // Current match is a copy
    auto match = [&target_pos, &m_it, &copy_func, &match_begin] () -> void {
      assert(m_it->len > 0);
      copy_func(std::distance(match_begin, m_it), m_it->ptr, m_it->len);
      target_pos = m_it->ptr + m_it->len;
      std::advance(m_it, m_it->len);
    };

    // Write literals up to target
    auto literal = [&m_it, &literal_func, &match_begin] (MatchIt target) -> void {
      auto len = std::distance(m_it, target);
      if (len > 0) {
        literal_func(std::distance(match_begin, m_it), len);
        m_it = target;
      }
    };

    // Magnetic match
    auto mag_match = [&] () -> bool {
      auto s_end = std::next(m_it, E_L);
      for (auto s_it = m_it; (s_it <= s_end) and (s_it < match_end); ++s_it) {
        auto int_tgt  = static_cast<int>(s_it->ptr);
        auto int_tp   = static_cast<int>(target_pos);
        if (s_it->matched() and static_cast<size_t>(std::abs(int_tgt - int_tp)) < E_L) {
            literal(s_it);
            match();
            return true;
        }
      }
      return false;
    };

    // Last-resort match
    auto flex_match = [&] () -> void {
      for (auto s_it = m_it; s_it != match_end; ++s_it) {
        if (s_it->len >= P_T) {
          literal(s_it);
          match();
          return;
        }
      }
      literal(match_end);
    };

    while (m_it < match_end) {
      if (!mag_match()) {
        flex_match();
      }
    }
    end_evt();
  }

};

template <typename Alphabet, typename ParseType = Parser, typename LiteralEvt, typename CopyEvt, typename EndEvt = std::function<void()>>
std::tuple<mapped_stream<Alphabet>, mapped_stream<Alphabet>> parse(
  const char *reference_path, const char *input_path,
  LiteralEvt lit_evt, CopyEvt copy_evt, EndEvt end_evt,
  size_t edit_distance = 30U, size_t phrase_threshold = 15U
)
{
  // Get matching stats
  std::vector<match> M;
  mapped_stream<Alphabet> input, reference; 
  {
    std::ifstream ref_ifs, input_ifs;
    io::open_file(ref_ifs, reference_path);
    io::open_file(input_ifs, input_path);
    rlz::utils::stream_dumper ref_dump(ref_ifs), input_dump(input_ifs);
    std::tie(M, reference, input) = get_relative_matches<Alphabet>(ref_dump, input_dump);
  }

  namespace chr = std::chrono;
  auto t_1 = chr::high_resolution_clock::now();
  Parser{edit_distance, phrase_threshold}(
    M.begin(), M.end(), reference.size(), lit_evt, copy_evt, end_evt
  );
  auto t_2 = chr::high_resolution_clock::now();
  std::cerr << "--- Parsing  = " 
            << chr::duration_cast<chr::microseconds>(t_2 - t_1).count()
            << " μs" << std::endl;
  return std::make_tuple(reference, input);
}

}
