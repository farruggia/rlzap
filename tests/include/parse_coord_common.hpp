#pragma once

#include <cassert>
#include <cstdint>
#include <iterator>
#include <random>
#include <sstream>
#include <vector>

#include <alphabet.hpp>
#include <impl/differential_iterator.hpp>
#include <dumper.hpp>
#include <sa_compute.hpp>

template <typename SourceIt, typename RefIt>
struct checker {
private:
  SourceIt src_begin, src_end;
  RefIt ref_begin, ref_end;
  using T = decltype(2 * *src_begin );

  std::size_t last_literal_end;
  T last_literal;


  T get_source(std::int64_t pos) {
    return *std::next(src_begin, pos);
  }

  T get_ref(std::int64_t pos) {
    return *std::next(ref_begin, pos);
  }  

public:
  checker(SourceIt src_begin, SourceIt src_end, RefIt ref_begin, RefIt ref_end)
    : src_begin(src_begin), src_end(src_end), ref_begin(ref_begin), ref_end(ref_end),
      last_literal_end(0U), last_literal(T{})
  { }

  void literal_evt(std::size_t pos, std::size_t len)
  {
    assert(len > 0);
    last_literal_end = pos + len;
    last_literal = get_source(pos + len - 1);
  }

  void copy_evt(std::size_t pos, std::size_t ref, std::size_t len)
  {
    // source_base, reference_base, reference_begin, reference_end
    std::vector<T> reconstructed(len, T{});
    auto rec_it = reconstructed.begin();
    T source_base, reference_base;
    RefIt reference_it;

    if (pos + len > std::distance(src_begin, src_end)) {
      std::stringstream ss;
      ss << "C(" << pos << ") <- " << ref << " @ " << len << ": "
         << "Source out-of-bound "
         << "(Length = " << std::distance(src_begin, src_end) << ")";
      throw std::logic_error(ss.str());      
    }

    if (ref + len > std::distance(ref_begin, ref_end)) {
      std::stringstream ss;
      ss << "C(" << pos << ") <- " << ref << " @ " << len << ": "
         << "Reference out-of-bound "
         << "(Length = " << std::distance(ref_begin, ref_end) << ")";
      throw std::logic_error(ss.str());                  
    }

    if (pos != last_literal_end) {
      source_base    = get_source(pos);
      reference_base = get_ref(ref);
      if (source_base != reference_base) {
        std::stringstream ss;
        ss << "C(" << pos << ") <- " << ref << " @ " << len << ": "
           << "special phrase with source/target mismatch "
           << "(S: " << source_base << ", "
           << "R: " << reference_base << ")";
        throw std::logic_error(ss.str());        
      }
      reference_it = std::next(ref_begin, ref + 1);
      *rec_it++ = source_base;
    } else {
      source_base     = last_literal;
      reference_base  = ref == 0 ? T{} : get_ref(ref - 1);
      reference_it = std::next(ref_begin, ref);
    }

    while (rec_it != reconstructed.end()) {
      *rec_it++ = source_base + *reference_it++ - reference_base;
    }

    auto s_start = std::next(src_begin, pos);
    auto s_end   = std::next(s_start, len);

    if (!std::equal(s_start, s_end, reconstructed.begin())) {
      std::stringstream ss;
      ss << "C(" << pos << ") <- " << ref << " @ " << len << ": "
         << "Invalid copy";
      throw std::logic_error(ss.str());                  
    }
  }
};

template <typename T = std::default_random_engine>
std::string gen_dna_like(const std::size_t length, T rg = T{42})
{
  const std::vector<char> alphabet {{'A', 'C', 'G', 'T'}};
  const char null_char      = 'N';
  const double null_prob    = 0.05;
  const size_t avg_null_len = 4UL;
  const size_t std_null_len = 2UL;

  std::vector<char> sol;
  sol.reserve(length);

  std::uniform_real_distribution<> gen_null_char;
  std::uniform_int_distribution<>  gen_alphabet(0, 3);
  std::normal_distribution<> null_len(avg_null_len, std_null_len);

  for (auto thus_far = 0UL; thus_far < length;) {
    if (gen_null_char(rg) > null_prob) {
      sol.push_back(alphabet[gen_alphabet(rg)]);
      ++thus_far;
    } else {
      auto len = std::min<std::size_t>(length - thus_far, null_len(rg));
      sol.insert(sol.end(), len, null_char);
      thus_far += len;
    }
  }
  return std::string(sol.begin(), sol.end());
}

template <typename T = std::default_random_engine>
void mutations(std::vector<char> &V, size_t amount, T rg = T{42})
{
  auto L = V.size();
  std::uniform_real_distribution<> gen_mut;
  std::uniform_int_distribution<> gen_pos(0, L - 1);
  std::normal_distribution<> del_len(2UL, 1.0);  
  std::normal_distribution<> ins_len(2UL, 1.0);
  std::normal_distribution<> mov_len(32UL, 1.5);


  auto deletion = [&] () {
    auto start_pos = gen_pos(rg);
    auto len       = std::min<size_t>(L, 1 + del_len(rg));
    // std::cout << "D(" << start_pos << " - " << start_pos + len << ")" << std::endl;
    V.erase(V.begin() + start_pos, V.begin() + start_pos + len);
    L -= len;
    gen_pos = std::uniform_int_distribution<>(0, L - 1);
  };

  auto insertion = [&] () {
    auto start_pos = gen_pos(rg);
    auto to_insert = gen_dna_like(1 + ins_len(rg), rg);
    // std::cout << "I(" << start_pos << "): <- " << to_insert << std::endl;
    V.insert(V.begin() + start_pos, to_insert.begin(), to_insert.end());
    L += to_insert.length();
    gen_pos = std::uniform_int_distribution<>(0, L - 1);
  };

  auto snip = [&] () {
    auto start_pos = gen_pos(rg);
    auto change    = gen_dna_like(1UL, rg);
    // std::cout << "S(" << start_pos << "): " << V[start_pos] << " <- " << change << std::endl;
    V[start_pos]   = *(change.begin());
  };

  auto move = [&] () {
    auto from = gen_pos(rg);
    auto len  = std::min<std::size_t>(1 + mov_len(rg), L - from);
    std::size_t to;
    do {
      to = gen_pos(rg);
    } while (to > L - len);

    // std::cout << "M(" << from << " - " << (from + len) << ") >- " << to << std::endl;
    std::string save{V.begin() + from, V.begin() + from + len};
    V.erase(V.begin() + from, V.begin() + from + len);
    V.insert(V.begin() + to, save.begin(), save.end());
  };

  std::vector<std::function<void()>> funcs;
  funcs.emplace_back(deletion);
  funcs.emplace_back(insertion);
  funcs.emplace_back(snip);
  funcs.emplace_back(move);

  std::vector<double> probs {{ 0.15, 0.15, 0.8, 1 }};


  while (amount-- > 0) {
    auto func_idx = std::distance(probs.begin(),std::lower_bound(probs.begin(), probs.end(), gen_mut(rg)));
    funcs[func_idx]();
  }
}

using LcpSym = typename rlz::alphabet::lcp_32::Symbol;
std::tuple<std::vector<LcpSym>, std::vector<LcpSym>> dna_sa_lcp(const std::string dna)
{
  sdsl::int_vector<> SA, LCP;
  std::vector<char> cont(dna.begin(), dna.end());
  cont.push_back('\0');
  rlz::sa_compute<char>{}(cont.data(), SA, LCP, cont.size());
  return std::make_tuple(
    std::vector<LcpSym>(SA.begin(), SA.end()),
    std::vector<LcpSym>(LCP.begin(), LCP.end())
  );
}

template <size_t Length>
class refinput_get
{
private:
  static std::string cached_reference;
  static std::string cached_input;
public:
  const std::string &reference()
  {
    if (cached_reference.empty()) {
      cached_reference = gen_dna_like(Length);
    }
    return cached_reference;
  }

  const std::string &input()
  {
    if (cached_input.empty()) {
      auto ref = this->reference();
      std::vector<char> mut(ref.begin(), ref.end());
      mutations(mut, Length * 0.01);
      cached_input = std::string(mut.data(), mut.size());      
    }
    return cached_input;
  }
};

template <size_t Length>
std::string refinput_get<Length>::cached_reference;

template <size_t Length>
std::string refinput_get<Length>::cached_input;

template <typename Iter>
using DiffIt = rlz::differential_iterator<Iter, std::int32_t>;

template <typename Iter>
using Dump = rlz::utils::iterator_dumper<DiffIt<Iter>>;

template <typename Iter>
Dump<Iter> get_differential(Iter begin, Iter end)
{
  return Dump<Iter>(DiffIt<Iter>(begin), DiffIt<Iter>(end));
}

template <size_t length, typename Symbol = std::uint32_t>
struct ds_getter {

  using iterator       = typename std::vector<Symbol>::iterator;
  using const_iterator = typename std::vector<Symbol>::const_iterator;

  std::vector<Symbol> &input()
  {
    if (_input.size() == 0U) {
      auto input_dna = getter.input();
      _input = std::get<1>(dna_sa_lcp(input_dna));
    }
    // std::cout << "--- Input length: " << _input.size() << std::endl;
    return _input;
  }

  std::vector<Symbol> &reference()
  {
    if (_reference.size() == 0U) {
      auto ref_dna = getter.reference();
      _reference = std::get<1>(dna_sa_lcp(ref_dna));
    }
    // std::cout << "--- Reference length: " << _reference.size() << std::endl;
    return _reference;
  }
private:
  refinput_get<length> getter;
  static std::vector<Symbol> _input;
  static std::vector<Symbol> _reference;
};

template <size_t length, typename Symbol>
std::vector<Symbol> ds_getter<length, Symbol>::_input;
template <size_t length, typename Symbol>
std::vector<Symbol> ds_getter<length, Symbol>::_reference;
