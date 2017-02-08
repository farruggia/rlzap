#pragma once

#include <algorithm>
#include <cstdlib>

#include "sdsl_extensions/int_vector.hpp"

#include "reference_wrap.hpp"
#include "impl/alphabet.hpp"
#include "type_name.hpp"


namespace rlz {

template <typename IntegerSize>
class integer_pack {
private:
  constexpr const static size_t bits = IntegerSize::value();
  sdsl::extensions::int_vector<bits> data;
public:
  integer_pack() { }

  using orig_iterator = typename sdsl::extensions::int_vector<bits>::const_iterator;
  using iterator      = impl::const_wrap_iterator<orig_iterator, typename rlz::alphabet::bits_traits<bits>::Type>;

  template <typename JunkIt>
  integer_pack(JunkIt begin, JunkIt end) : data(std::distance(begin, end), 0UL)
  {
    std::copy(begin, end, data.begin());
  }

  iterator from(std::size_t idx) const { return std::next(begin(), idx); }

  iterator begin() const { return iterator(data.begin()); }

  iterator end() const { return iterator(data.end()); }

  size_t size() const { return data.size(); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return data.serialize(out, v, name);
  }

  void load(std::istream &in)
  {
    data.load(in);
  }

};

}