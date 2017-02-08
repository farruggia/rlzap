#pragma once

#include <algorithm>
#include <cassert>

#include <boost/iterator/iterator_adaptor.hpp>

#include "sdsl_extensions/lcp_byte.hpp"
#include "check_random.hpp"
#include "type_name.hpp"

namespace rlz {

template <typename Symbol>
class lcp_pack {

  sdsl::extensions::signed_lcp_byte<Symbol> junks;

public:

  using iterator = typename sdsl::extensions::signed_lcp_byte<Symbol>::const_iterator;
  CHECK_TYPE_RANDOM(iterator, "lcp_pack: iterator is not random");

  lcp_pack() { }

  template <typename JunkIt>
  lcp_pack(JunkIt junk_begin, JunkIt junk_end) 
    : junks(junk_begin, junk_end)
  {
      assert(std::equal(junk_begin, junk_end, junks.begin()));
  }

  iterator from(size_t idx) const
  {
    return std::next(junks.begin(), idx);
  }

  Symbol operator[](size_t idx) const
  {
    return junks[idx];
  }


  iterator begin() const
  {
    return junks.begin();
  }

  iterator end() const
  {
    return junks.end();
  }

  size_t size() const { return junks.size(); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return junks.serialize(out, v, name);
  }

  void load(std::istream& in)
  {
    junks.load(in);
  }
};
}
