#pragma once

#include <cstdint>
#include <iterator>

#include <boost/iterator/iterator_facade.hpp>

#include <sdsl/io.hpp>

#include "check_random.hpp"
#include "type_name.hpp"

namespace rlz { namespace classic { namespace prefix {

class trivial {
  std::uint32_t phrases;
public:

  class iterator : public boost::iterator_facade<trivial::iterator, const size_t, boost::random_access_traversal_tag>
  {
    const static size_t val_ret = 1UL;
    size_t idx;
  public:
    iterator(size_t idx) : idx(idx) { }
  private:
    friend class boost::iterator_core_access;

    bool equal(trivial::iterator const& other) const
    {
        return this->idx == other.idx;
    }

    void increment() { ++idx; }

    void decrement() { --idx; }

    void advance(int n) { idx += n; }

    std::ptrdiff_t distance_to(trivial::iterator const& other) const
    {
      return static_cast<std::ptrdiff_t>(idx) - static_cast<std::ptrdiff_t>(other.idx);
    }

    const size_t& dereference() const { return val_ret; }

  };

  static constexpr const size_t max_length = 1UL;


  CHECK_TYPE_RANDOM(iterator, "trivial::iterator is not random access");

  trivial() { }

  template <typename It>
  trivial(It begin, It end)
    : phrases(std::distance(begin, end))
  {
  }

  size_t access(size_t index) const { return 1UL; }

  size_t prefix(size_t index) const { return index + 1; }

  iterator get_iterator(size_t index) const
  {
    return iterator(index);
  }

  iterator get_iterator_end() const
  {
    return iterator(phrases);
  }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return sdsl::write_member(phrases, out, v, name);
  }

  void load(std::istream& in)
  {
    sdsl::read_member(phrases, in);
  }
};

}}}