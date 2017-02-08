#pragma once


#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/zip_iterator.hpp>

#include "bit_vectors.hpp"
#include "check_random.hpp"
#include "integer_type.hpp"
#include "prefix_sum.hpp"
#include "sparse_dense_vector.hpp"
#include "sdsl_extensions/int_vector.hpp"
#include "type_name.hpp"

#include <array>
#include <cassert>
#include <iterator>
#include <vector>

namespace rlz {

template <typename BlockSize = rlz::values::Size<8U>>
class dna_pack {
private:
  sdsl::extensions::int_vector<2U>              literals;
  vectors::sd_bitvector<BlockSize>  n_marker;
  static constexpr std::array<size_t, 10> char_to_bit {{0, 1, 0, 2, 0, 0, 0, 0, 0, 3}};
  static constexpr std::array<char, 4> bit_to_char {{ 'A', 'C', 'G', 'T' }};
  static constexpr char Nref {'N'};
  using orig_iterator   = sdsl::extensions::int_vector<2U>::const_iterator;
  using filter_iterator = typename vectors::sd_bitvector<BlockSize>::iterator;
  CHECK_TYPE_RANDOM(orig_iterator, "dna_pack::orig_iterator is not randm-access");
  CHECK_TYPE_RANDOM(filter_iterator, "dna_pack::filter_iterator is not randm-access");

  using orig_value_type = typename std::iterator_traits<orig_iterator>::value_type;

  struct join {
    const char &operator()(const boost::tuple<const bool&, const orig_value_type&> &t) const
    {
      return t.get<0>() ? Nref : bit_to_char[t.get<1>()];
    }
  };

  struct to_bit {
    size_t operator()(char c) const
    {
      return char_to_bit[(c - 'A') >> 1];
    }
  };


  using zip_iterator = boost::zip_iterator<boost::tuple<filter_iterator, orig_iterator>>;
  using iterator_seq = boost::transform_iterator<join, zip_iterator>;

public:

  class iterator;
  CHECK_TYPE_RANDOM(iterator, "dna_pack::iterator is not random-access");

  dna_pack() { }

  template <typename JunkIt>
  dna_pack(JunkIt junk_begin, JunkIt junk_end)
    : literals(std::distance(junk_begin, junk_end))
  {
    // Copy junks
    using view_iterator = boost::transform_iterator<to_bit, JunkIt>;
    std::copy(
          view_iterator(junk_begin, to_bit{}),
          view_iterator(junk_end, to_bit{}),
          literals.begin()
    );

    // Handle N
    std::vector<size_t> markers;
    for (auto it = junk_begin; it < junk_end; ++it) {
      if (*it == 'N') { markers.push_back(std::distance(junk_begin, it)); }
    }
    n_marker = vectors::sd_bitvector<BlockSize>(markers.begin(), markers.end(), literals.size());
  }

  iterator from(size_t idx) const
  {
    auto zip_it  = boost::make_zip_iterator(boost::make_tuple(
      n_marker.from(idx),
      std::next(literals.begin(), idx)
    ));
    return iterator { boost::make_transform_iterator(zip_it, join{}) };

  }

  const char &operator[](size_t idx) const
  {
    return *from(idx);
  }


  iterator begin() const
  {
    return from(0U);
  }

  iterator end() const
  {
    auto zip_it  = boost::make_zip_iterator(boost::make_tuple(
      n_marker.end(),
      literals.end()
    ));
    return iterator { boost::make_transform_iterator(zip_it, join{}) };
  }

  size_t size() const { return literals.size(); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += literals.serialize(out, child, "Junk characters");
    written_bytes += n_marker.serialize(out, child, "N markers");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    literals.load(in);
    n_marker.load(in);
  }

};

template <typename BlockSize>
constexpr std::array<size_t, 10> dna_pack<BlockSize>::char_to_bit;
template <typename BlockSize>
constexpr std::array<char, 4> dna_pack<BlockSize>::bit_to_char;
template <typename BlockSize>
constexpr char dna_pack<BlockSize>::Nref;

template <typename BlockSize>
class dna_pack<BlockSize>::iterator
  : public boost::iterator_adaptor<
    typename dna_pack<BlockSize>::iterator,
    typename dna_pack<BlockSize>::iterator_seq,
    boost::use_default,
    std::random_access_iterator_tag
  >
{
public:
  // Delegating constructor
  using boost::iterator_adaptor<
    typename dna_pack<BlockSize>::iterator,
    typename dna_pack<BlockSize>::iterator_seq,
    boost::use_default, std::random_access_iterator_tag
  >::iterator_adaptor;
};

}
