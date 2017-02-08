#pragma once

#include <boost/iterator/transform_iterator.hpp>
#include <boost/range.hpp>

#include <sdsl/util.hpp>

#include "bit_vectors.hpp"
#include "check_random.hpp"
#include "diff_iterator.hpp"
#include "fractional_container.hpp"
#include "integer_type.hpp"
#include "sdsl_extensions/int_vector.hpp"
#include "impl/prefix_sum.hpp"
#include "static_table_generation.hpp"
#include "type_name.hpp"

namespace rlz { namespace prefix {

template <typename Rep, typename LengthWidth = values::Size<4U>>
class fast_cumulative {
private:
  using BitVector = vectors::BitVector<Rep, vectors::algorithms::select>;
  BitVector                       cumulative;
  sdsl::extensions::int_vector<LengthWidth::value()>  lit_lengths;
public:

  static constexpr size_t max_length = (1ULL << LengthWidth::value()) - 1;

  using iterator = typename sdsl::extensions::int_vector<LengthWidth::value()>::const_iterator;

  CHECK_TYPE_RANDOM(iterator, "fast_cumulative::iterator is not random access");

  fast_cumulative() { }

  template <typename It>
  fast_cumulative(It begin, It end)
    : lit_lengths(std::distance(begin, end), 0U)
  {
    size_t length = 0U;
    for (auto i : boost::make_iterator_range(begin, end)) { length += i; }
    typename BitVector::builder build(length + lit_lengths.size() + 1);
    auto cumulative_idx = 0U;
    for (auto i = 0U; i < lit_lengths.size(); ++i) {
      auto len = *begin++;
      lit_lengths[i] = len;
      cumulative_idx += len + 1;
      build.set(cumulative_idx);
    }
    cumulative   = BitVector(build.get());
  }

  size_t access(size_t index) const { return lit_lengths[index]; }

  size_t prefix(size_t index) const
  {
    auto idx = index + 1;
    return cumulative.select_1(idx) - idx;
  }

  iterator get_iterator(size_t index) const
  {
    return std::next(lit_lengths.begin(), index);
  }

  iterator get_iterator_end() const
  {
    return lit_lengths.end();
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += cumulative.serialize(out, child, "cumulative");
    written_bytes += lit_lengths.serialize(out, child, "lit_lengths");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    cumulative.load(in);
    lit_lengths.load(in);
  }
};

template <
  typename LengthWidth = values::Size<2U>,
  typename SyncSampling = values::Size<16U>,
  typename SamplingSize = values::Size<32U>
>
class sampling_cumulative {
private:
  using TableType = typename rlz::static_table::generate_array<256U, impl::table_gen::Generators<LengthWidth::value()>:: template gen>::result;
  using lit_type  = rlz::fractional_byte_container<LengthWidth::value()>;
  using lit_it    = typename lit_type::iter;
  lit_type lit_lengths;
  sdsl::int_vector<SamplingSize::value()> sync_lengths;


  static const constexpr size_t ElementsPerByte = 8UL / LengthWidth::value();
  static const constexpr size_t BytesPerSync    = SyncSampling::value() / ElementsPerByte;

  using MaskType  = typename rlz::static_table::generate_array<ElementsPerByte, impl::mask_gen::Generators<LengthWidth::value()>:: template gen>::result;

  static_assert((SyncSampling::value() % ElementsPerByte) == 0, "SyncSampling must be a multiple of elements per byte");

  std::uint16_t combine(std::uint8_t v, size_t idx) const
  {
    return v & MaskType::data[idx];
  }


public:
  static constexpr size_t max_length = (1ULL << LengthWidth::value()) - 1UL;

  using iterator = lit_it;

  sampling_cumulative() { }

  template <typename It>
  sampling_cumulative(It begin, It end)
    : lit_lengths(begin, end), 
      sync_lengths(1UL + std::distance(begin, end) / SyncSampling::value(), 0U)
  {
    auto cumulative = 0U;
    for (auto i = 0U; i < lit_lengths.size(); ++i) {
      auto len = *begin++;
      if ((i % SyncSampling::value()) == 0) {
        sync_lengths[i / SyncSampling::value()] = cumulative;
      }
      cumulative += len;
    }
  }

  size_t access(size_t index) const { return lit_lengths[index]; }

  size_t prefix(size_t index) const
  {
    auto smp_idx  = index / SyncSampling::value();
    auto sum      = sync_lengths[smp_idx];
    auto evals    = index % SyncSampling::value();
    auto vals_ptr = std::next(lit_lengths.data(), smp_idx * BytesPerSync);
    for (; evals >= ElementsPerByte; evals -= ElementsPerByte) {
      sum += TableType::data[*vals_ptr++];
    }
    return sum + TableType::data[combine(*vals_ptr, evals)];
  }

  lit_it get_iterator(size_t index) const
  {
    return std::next(lit_lengths.begin(), index);
  }

  lit_it get_iterator_end() const
  {
    return lit_lengths.end();
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += lit_lengths.serialize(out, child, "lit_lengths");
    written_bytes += sync_lengths.serialize(out, child, "sync_lengths");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    lit_lengths.load(in);
    sync_lengths.load(in);
  }

};

template <typename Rep = vectors::sparse, typename LengthWidth = values::Size<4U>>
class cumulative {
private:
  using BitVector       = vectors::BitVector<Rep, vectors::algorithms::select, vectors::algorithms::bitset>;
  BitVector cum_bv;
  using bitset_iterator = decltype(cum_bv.get_bitset(0U));
  size_t    elements;

  struct differ { 
    size_t operator()(size_t e) const 
    {
      assert(e > 0);
      return e - 1;
    }
  };

  using diff_iterator = utils::iterator_diff<bitset_iterator, size_t>;

public:

  static constexpr size_t max_length = (1ULL << LengthWidth::value()) - 1;
  using iterator = boost::transform_iterator<differ, diff_iterator>;

  cumulative() { }

  template <typename It>
  cumulative(It begin, It end) : elements(std::distance(begin, end))
  {
    size_t bits_set = std::distance(begin, end);
    size_t length = 0U;
    for (auto i : boost::make_iterator_range(begin, end)) { length += i; }
    typename BitVector::builder build(length + bits_set + 1);
    auto cumulative_idx = 0U;
    for (auto i : boost::make_iterator_range(begin, end)) {
      cumulative_idx += i + 1;
      build.set(cumulative_idx);      
    }
    cum_bv = BitVector(build.get());
  }

  size_t access(size_t index) const
  {
    return (index == 0) ? prefix(index) : prefix(index) - prefix(index - 1);
  }

  size_t prefix(size_t index) const
  {
    auto idx = index + 1;
    return cum_bv.select_1(idx) - idx;
  }

  iterator get_iterator(size_t index) const
  {
    static_assert(std::is_same<decltype(cum_bv.get_bitset(index)), bitset_iterator>::value, "get_bitset mismatch");
    auto prev = (index == 0) ? 0 : prefix(index - 1) + index;
    return iterator(diff_iterator(prev, cum_bv.get_bitset(index)), differ{});
  }

  iterator get_iterator_end() const
  {
    static_assert(std::is_same<decltype(cum_bv.get_bitset_end()), bitset_iterator>::value, "get_bitset mismatch");
    return iterator(diff_iterator::end_iterator(cum_bv.get_bitset_end()), differ{});
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += cum_bv.serialize(out, child, "cumulative");
    written_bytes += sdsl::write_member(elements, out, child, "elements");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    cum_bv.load(in);
    sdsl::read_member(elements, in);
  }
};

}}
