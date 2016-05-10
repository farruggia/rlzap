#pragma once

#include <memory>
#include <vector>

#include "reference_wrap.hpp"
#include "type_utils.hpp"

namespace rlz {

template <typename T>
class mapped_array {
private:
  std::shared_ptr<std::vector<T>> storage;
  T *beg;
  size_t len;
public:

  mapped_array(
    std::shared_ptr<std::vector<T>> storage,
    size_t start_pos,
    size_t len
  ) : storage(storage), beg(storage->data() + start_pos), len(len)
  {

  }

  mapped_array() : storage(nullptr), beg(nullptr), len(0U) { }

  T *begin() const { return beg; }
  T *end()   const { return beg + len; }

  T &operator[](size_t pos) const { return beg[pos]; }
  size_t size() const { return len; }
  T *data() const { return beg; };
};

template <typename Alphabet>
using mapped_stream = mapped_array<typename Alphabet::Symbol>;

struct null_ownership { };

template <typename T>
class shared_ownership {
  std::shared_ptr<T> ptr;
public:
  shared_ownership() { }
  shared_ownership(std::shared_ptr<T> ptr) : ptr(ptr) { }

  shared_ownership(std::unique_ptr<T> ptr) : ptr(ptr.release()) { }

  shared_ownership(std::unique_ptr<T[]> ptr) : ptr(ptr.release(), std::default_delete<T[]>{}) { }

  T *get() { return ptr.get(); }
};

template <typename Alphabet, typename Iterator, typename Ownership = null_ownership>
class iterator_container {
private:
  using Symbol   = typename Alphabet::Symbol;
  using IterType = rlz::impl::const_wrap_iterator<Iterator, Symbol>;
  IterType begin_;
  IterType end_;
  Ownership own;
public:
  using reference_type = Symbol;

  iterator_container() { }
  iterator_container(Iterator begin_, Iterator end_) : begin_(begin_), end_(end_) { }

  template <typename Own>
  iterator_container(Iterator begin_, Iterator end_, Own &&own) 
    : begin_(begin_), end_(end_), own(std::forward<Own>(own))
  { }

  IterType begin() const { return begin_; }
  IterType end() const { return end_; }

  reference_type operator[](size_t pos) const { return *std::next(begin_, pos); }
  size_t size() const { return std::distance(begin(), end()); }
};

template <typename Alphabet, typename Ownership = null_ownership, typename Iterator>
iterator_container<Iterator, Ownership> get_iterator_container(Iterator begin, Iterator end, Ownership own = Ownership{})
{
  return iterator_container<Alphabet, Iterator, Ownership>(begin, end, own);
}

template <typename Alphabet, typename Reference>
class container_wrapper {
  Reference r;
  using Symbol    = typename Alphabet::Symbol;
  using OrigIter  = rlz::type_utils::RemoveQualifiers<decltype(const_cast<const Reference&>(r).begin())>;
public:

  using Iterator = rlz::impl::const_wrap_iterator<OrigIter, Symbol>;

  container_wrapper() { }

  container_wrapper(Reference r) : r(r) { }

  Iterator begin() const { return Iterator(r.begin()); }
  Iterator end() const   { return Iterator(r.end()); }
  Symbol operator[](size_t pos) const { return r[pos]; }
  size_t size() const { return r.size(); }
};

template <typename Alphabet>
class text_wrap {
public:
  using Symbol = typename Alphabet::Symbol;
private:
  const Symbol *begin_;
  const Symbol *end_;
public:
  text_wrap() : begin_(nullptr), end_(nullptr) { }
  text_wrap(const Symbol *ptr, size_t length) : begin_(ptr), end_(ptr + length)
  { }

  Symbol operator[](size_t idx) const { return begin_[idx]; }
  const Symbol *begin() const { return begin_; }
  const Symbol *end() const { return end_; }
  size_t size() const { return end_ - begin_; }
};

template <typename Alphabet>
class managed_wrap {
public:
  using Symbol = typename Alphabet::Symbol;
private:
  std::shared_ptr<Symbol> data;
  Symbol *begin_;
  Symbol *end_;
public:
  managed_wrap() : data(nullptr), begin_(nullptr), end_(nullptr) { }
  managed_wrap(std::shared_ptr<Symbol> data, size_t length) 
    : data(data), begin_(data.get()), end_(data.get() + length)
  { }

  Symbol operator[](size_t idx) const { return begin_[idx]; }
  const Symbol *begin() const { return begin_; }
  const Symbol *end() const { return end_; }
  size_t size() const { return end_ - begin_; }
};

}