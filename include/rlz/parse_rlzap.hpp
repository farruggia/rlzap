#pragma once

#include "dumper.hpp"
#include "io.hpp"
#include "get_matchings.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <tuple>

namespace rlz {

class parser_rlzap {
private:
    const size_t       rlt_bits; // Relative pointer length (in bits)
    const size_t       abs_bits; // Absolute pointer length (in bits)
    const size_t       sym_bits; // Symbol length (in bits)
    const std::int64_t rlt_hi;   // Maximum diff value + 1
    const std::int64_t rlt_lo;   // Minimum low value - 1
    const size_t       max_lit;  // Max magnetic literal length
    const size_t       min_rel;  // Min magnetic copy length
    const size_t       min_abs;  // Min absolute copy length
public:

  // "Parametric" parser
  parser_rlzap(size_t rlt_bits, size_t abs_bits, size_t sym_bits, size_t edit_distance, size_t threshold)
    : rlt_bits(rlt_bits), abs_bits(abs_bits), sym_bits(sym_bits), 
      rlt_hi(1L << rlt_bits), rlt_lo(-(rlt_hi + 1L)),
      max_lit(edit_distance), min_rel(rlt_bits / sym_bits), min_abs(threshold)
  { }

  // "Automatic" parser
  parser_rlzap(size_t rlt_bits, size_t abs_bits, size_t sym_bits) 
    : parser_rlzap(rlt_bits, abs_bits, sym_bits, (abs_bits - rlt_bits) / sym_bits, abs_bits / sym_bits)
  { }

  // Convenience default constructor
  parser_rlzap()
    : parser_rlzap(4UL, 32UL, 2UL)
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
    auto m_it      = match_begin;                                  // Current input position
    size_t ref_pos = std::numeric_limits<size_t>::max() - max_lit; // Current reference position by following 
                                                                   // last absolute ptr
    // Current match is a copy
    auto match = [&ref_pos, &m_it, &copy_func, &match_begin] () -> void {
      assert(m_it->len > 0);
      copy_func(std::distance(match_begin, m_it), m_it->ptr, m_it->len);
      ref_pos += m_it->len;
      std::advance(m_it, m_it->len);
    };

    // Write literals up to target
    auto literal = [&m_it, &literal_func, &match_begin, &ref_pos] (MatchIt target) -> void {
      auto len = std::distance(m_it, target);
      ref_pos += len;
      if (len > 0) {
        literal_func(std::distance(match_begin, m_it), len);
        m_it = target;
      }
    };

    // Pointer is relative?
    auto is_relative = [this] (size_t abs_source, size_t target) {
      auto delta = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(abs_source);
      return (delta > rlt_lo) and (delta < rlt_hi);
    };

    // Magnetic match
    auto mag_match = [&] (MatchIt s_it, size_t c_ref_pos, bool apply) -> bool {
      // First character is a mandatory literal
      auto s_end = std::next(s_it, max_lit);
      while ((s_it <= s_end) and (s_it < match_end)) {
        if (s_it->len >= min_rel and is_relative(c_ref_pos, s_it->ptr)) {
          if (apply) {
            literal(s_it);
            match();            
          }
          return true;
        }
        // Another literal
        ++c_ref_pos; ++s_it;
      }
      return false;
    };

    // Last-resort match
    auto flex_match = [&] () -> void {
      for (auto s_it = m_it; s_it != match_end; ++s_it) {
        if (s_it->matched() and (
							(s_it->len >= min_abs) or 
							mag_match(std::next(s_it, s_it->len), s_it->ptr + s_it->len, false)
						))
				{
          literal(s_it);
          ref_pos = s_it->ptr;
          match();
          return;
        }
        ++ref_pos;
      }
      literal(match_end);
    };

    while (m_it < match_end) {
      if (!mag_match(m_it, ref_pos, true)) {
        flex_match();
      }
    }
    end_evt();
  }

};

}
