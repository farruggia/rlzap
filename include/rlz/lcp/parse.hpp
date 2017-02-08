#pragma once

#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <functional>
#include <iterator>
#include <type_traits>
#include <tuple>

namespace rlz { namespace lcp {

class parser {
private:
    const std::int64_t rlt_hi;   // Maximum diff value + 1
    const std::int64_t rlt_lo;   // Minimum low value - 1
    const std::size_t  max_lit;  // Max magnetic literal length
    const std::size_t  min_rel;  // Min magnetic copy length
    const std::size_t  min_abs;  // Min absolute copy length
public:

  // "Parametric" parser
  parser(
    std::size_t rlt_bits, std::size_t abs_bits, std::size_t sym_bits,
    std::size_t edit_distance, std::size_t threshold
  )
    : rlt_hi(1L << rlt_bits), rlt_lo(-(rlt_hi + 1L)),
      max_lit(edit_distance),
      min_rel(std::max<size_t>(1, rlt_bits / sym_bits)),
      min_abs(std::max<size_t>(1, threshold))
  { }

  // "Automatic" parser
  parser(std::size_t rlt_bits, std::size_t abs_bits, std::size_t sym_bits) 
    : parser(rlt_bits, abs_bits, sym_bits, (abs_bits - rlt_bits) / sym_bits, abs_bits / sym_bits)
  { }

  // Convenience default constructor
  parser()
    : parser(4UL, 32UL, 2UL)
  { }
    
  template <
    typename MatchIt, typename SourceIt, typename RefIt,
    typename LiteralEvt, typename CopyEvt, typename EndEvt = std::function<void()>
  >
  void operator() (
    MatchIt match_begin, MatchIt match_end,
    SourceIt source_begin, SourceIt source_end,
    RefIt ref_begin, RefIt ref_end,
    LiteralEvt literal_func, CopyEvt copy_func, EndEvt end_evt
  ) const
  {
    using SourceT    = decltype(*source_begin * 2);
    using ReferenceT = decltype(*ref_begin * 2);
    static_assert(
      std::is_same<SourceT, ReferenceT>::value,
      "source and reference iterators point to elements of different types"
    );

    auto m_it      = match_begin;                                  // Current input position
    size_t ref_pos = std::numeric_limits<size_t>::max() - max_lit; // Current reference position by following 
                                                                   // last absolute ptr
    // Current match is a copy
    auto expl_match = [&ref_pos, &m_it, &copy_func, &match_begin] (
      std::size_t source_pos, std::size_t c_ref_pos, std::size_t length
    ) -> void {
      assert(length > 0);
      copy_func(source_pos, c_ref_pos, length);

      ref_pos += length;
      std::advance(m_it, length);
    };

    auto match = [&] () -> void {
      expl_match(std::distance(match_begin, m_it), m_it->ptr, m_it->len);
    };

    // Write literals up to target
    auto literal = [&m_it, &literal_func, &match_begin, &ref_pos] (MatchIt target) {
      auto len = std::distance(m_it, target);
      ref_pos += len;
      if (len > 0) {
        literal_func(std::distance(match_begin, m_it), len);
        m_it = target;
      }
    };

    auto extended_match = [&] () -> std::tuple<std::size_t, std::int64_t> {
      std::tuple<std::size_t, std::int64_t> empty {0UL, 0};
      if (not std::next(m_it)->matched()) {
        return empty;
      }
      std::int64_t pos            = std::distance(match_begin, m_it),
                   next_match_pos = std::next(m_it)->ptr,
                   next_pos       = pos + 1,
                   delta          = next_match_pos - next_pos;
      auto next_source    = *std::next(source_begin, pos);
      auto next_reference = *std::next(ref_begin, pos + delta);
      return (next_source == next_reference) 
              ? std::tuple<std::size_t, std::int64_t>{1UL + std::next(m_it)->len, delta}
              : empty;
    };

    // Pointer is relative?
    auto is_delta = [this] (size_t abs_source, size_t target) {
      auto delta = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(abs_source);
      return (delta > rlt_lo) and (delta < rlt_hi);
    };

    // Delta match
    auto delta_match = [&] (MatchIt s_it, size_t c_ref_pos, bool apply) -> bool {
      if (m_it == s_it) {
        std::size_t  ext_length;
        std::int64_t ext_delta;
        std::tie(ext_length, ext_delta) = extended_match();
        auto source_pos = std::distance(match_begin, m_it);
        if (ext_length > min_rel and is_delta(c_ref_pos, source_pos + ext_delta)) {
          if (apply) {
            expl_match(source_pos, source_pos + ext_delta, ext_length);
          }
          return true;
        }        
      }
      ++c_ref_pos;
      std::advance(s_it, 1);
      auto s_end = std::next(s_it, max_lit);
      while ((s_it <= s_end) and (s_it < match_end)) {
        if (s_it->len >= min_rel and is_delta(c_ref_pos, s_it->ptr)) {
          if (apply) {
            literal(s_it);
            match();            
          }
          return true;
        }
        // Another literal
        ++c_ref_pos;
        std::advance(s_it, 1);
      }
      return false;
    };

    auto is_relative = [&] (MatchIt s_it) {
      return s_it->len >= min_abs or (
        s_it->matched() and
        delta_match(std::next(s_it, s_it->len), s_it->ptr + s_it->len, false)
      );
    };

    // Relative match
    auto relative_match = [&] () -> void {
      {
        std::size_t  ext_length;
        std::int64_t ext_delta;
        std::tie(ext_length, ext_delta) = extended_match();
        auto pos = std::distance(match_begin, m_it);
        if (
          ext_length >= min_abs or 
          (ext_length > 0 and delta_match(std::next(m_it, ext_length), pos + ext_length, false))
        ) {
            ref_pos = pos + ext_delta;
            expl_match(pos, pos + ext_delta, ext_length);
            return;        
        }        
      }
      for (auto s_it = std::next(m_it); s_it != match_end; ++s_it) {
        if (is_relative(s_it)) {
          ref_pos = s_it->ptr;
          literal(s_it);
          match();
          return;
        }
        ++ref_pos;
      }
      literal(match_end);
    };

    relative_match();
    while (m_it < match_end) {
      if (!delta_match(m_it, ref_pos, true)) {
        relative_match();
      }
    }
    end_evt();
  }

};

}}