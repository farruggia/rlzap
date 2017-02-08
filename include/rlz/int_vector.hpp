#pragma once

#include "check_random.hpp"
#include "impl/int_vector.hpp"
#include "integer_type.hpp"
#include "sdsl_extensions/int_vector.hpp"
#include "type_name.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include <boost/iterator/iterator_adaptor.hpp>


namespace rlz {
namespace ds {

/////////////////////////////////////////// REFERENCE ///////////////////////////////////////////
template <typename Reference, size_t Width>
class const_int_vector_reference {
protected:
  Reference ref;
public:
  const_int_vector_reference(Reference ref) : ref(ref) { }

  /* Beware: values are NOT cached!
   * Use:
   *   int val  = ref;
   * instead of:
   *   auto val = ref;
   * if val has to be evaluated many times.
   * We don't precompute the result because the value
   * might not be needed (as in ref = 42).
   */
  operator int() const {
    std::uint64_t v = ref;
    return impl::sign<Width>{}(v);
  }
};

template <typename Reference, size_t Width>
class int_vector_reference : public const_int_vector_reference<Reference, Width> {
  using Base = const_int_vector_reference<Reference, Width>;
public:
  using Base::const_int_vector_reference;

  int_vector_reference<Reference, Width> &operator=(const int &value)
  {
    auto val  = impl::unsign<Width>{}(value);
    Base::ref = val;
    return *this;
  }
};

//////////////////////////////////////////// ITERATOR ///////////////////////////////////////////
template <typename Base, typename Value, typename Reference>
class signed_iterator
  : public boost::iterator_adaptor<
        signed_iterator<Base, Value, Reference>, 
        Base, 
        Value, 
        std::random_access_iterator_tag, // Cheating, but who cares
        Reference
    >
{
public:
  using boost::iterator_adaptor<signed_iterator<Base, Value, Reference>, Base, Value, std::random_access_iterator_tag, Reference>::iterator_adaptor;

private:
  friend class boost::iterator_core_access;

  Reference dereference() const
  {
    return Reference(*(this->base()));
  }
};

// Provide signed succinct vectors on top of sdsl unsigned succinct vectors
template <size_t Width, typename LoadFactor = values::Rational<15, 10>, size_t MinimumLoad = 128U>
class int_vector {
private:
  using vec_t  = sdsl::extensions::int_vector<Width>;
  vec_t  data;
  size_t size_;

  using sd_reference    = typename vec_t::reference;
  using sd_const_ref    = typename vec_t::const_reference;
  using sd_iter         = typename vec_t::iterator;
  using sd_const_iter   = typename vec_t::const_iterator;  
public:
  using reference       = int_vector_reference<sd_reference, Width>;
  using const_reference = const_int_vector_reference<sd_const_ref, Width>;  
  using iterator        = signed_iterator<sd_iter, int, reference>;
  using const_iterator  = signed_iterator<sd_const_iter, int, const_reference>;
  CHECK_TYPE_RANDOM(iterator, "int_vector::iterator is not random access");
  CHECK_TYPE_RANDOM(const_iterator, "int_vector::const_iterator is not random access");

  int_vector(size_t size, std::int64_t value) : data(size, value), size_(size)
  {
    std::fill(begin(), end(), value);
  }

  int_vector() : int_vector(0U, 0) { }

  // Access operators
  reference operator[](const size_t &idx)
  { 
    assert(idx < size()); 
    return reference(data[idx]);
  }
  
  const_reference operator[](const size_t &idx) const
  { 
    assert(idx < size()); 
    return const_reference(data[idx]);
  }

  // Iterators
  iterator begin() { return iterator(data.begin()); }
  iterator end() { return iterator(std::next(data.begin(), size())); }
  const_iterator begin() const { return const_iterator(data.begin()); }
  const_iterator end() const { return const_iterator(std::next(data.begin(), size())); }

  // Proxied calls to data. Only define what's needed
  void resize(const size_t new_size) { size_ = std::min(size_, new_size); data.resize(new_size); }

  // Vector-like interface
  size_t capacity() const { return data.size(); }
  size_t size() const { return size_; }
  bool empty() const { return data.empty(); }
  void push_back(const std::int64_t &value)
  {
    if (size() >= capacity()) {
      resize(std::max<size_t>(capacity() * LoadFactor::value(), MinimumLoad));
    }
    (*this)[size_++] = value;
  }

  reference back() { return (*this)[size_ - 1]; }
  const_reference back() const { return (*this)[size_ - 1]; }
  void shrink_to_fit() { resize(size()); }

  // Serialization functions
  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    size_t written_bytes = 0;
    written_bytes += data.serialize(out, child, "data");
    written_bytes += sdsl::write_member(size_, out, child, "size");
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    data.load(in);
    sdsl::read_member(size_, in);
  }

};
}
}