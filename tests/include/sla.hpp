#pragma once

#include <cstdint>
#include <sdsl/int_vector_buffer.hpp>

namespace relative {

template<class IntegerType>
inline std::uint64_t bit_length(IntegerType val)
{
  return sdsl::bits::hi(val) + 1;
}

class SLArray
{
public:
  using size_type  = std::uint64_t;
  using value_type = std::uint64_t;

  const static size_type  BLOCK_SIZE  = 64;
  const static value_type LARGE_VALUE = 255;

  SLArray();
  SLArray(const SLArray& s);
  SLArray(SLArray&& s);
  ~SLArray();

  template<class IntVector>
  explicit SLArray(const IntVector& source)
  {
    this->buildFrom(source);
  }

  // sdsl::int_vector_buffer cannot be const.
  explicit SLArray(sdsl::int_vector_buffer<0>& source);

  void swap(SLArray& s);
  SLArray& operator=(const SLArray& s);
  SLArray& operator=(SLArray&& s);

  size_type serialize(std::ostream& out, sdsl::structure_tree_node* v = nullptr, std::string name = "") const;
  void load(std::istream& in);

  inline size_type size() const { return this->small.size() - 1; }
  inline size_type largeValues() const { return this->large.size(); }

//------------------------------------------------------------------------------

  inline value_type operator[] (size_type i) const
  {
    if(this->small[i] < LARGE_VALUE) { return this->small[i]; }
    else { return this->large[this->large_rank(i)]; }
  }

  /*
    These versions provide faster sequential access to the array.
    Initialize rank with initForward/initBackward and iterate over successive
    positions with the same rank variable.
  */

  inline size_type initForward(size_type i) const { return this->large_rank(i); }
  inline size_type initBackward(size_type i) const { return this->large_rank(i + 1); }

  inline value_type accessForward(size_type i, size_type& rank) const
  {
    return (this->small[i] < LARGE_VALUE ? this->small[i] : this->large[rank++]);
  }

  inline value_type accessBackward(size_type i, size_type& rank) const
  {
    return (this->small[i] < LARGE_VALUE ? this->small[i] : this->large[--rank]);
  }

  /*
    Semiopen interval [from, to). Minimal sanity checking.
  */
  sdsl::int_vector<64> extract(size_type from, size_type to) const;

  inline size_type large_rank(size_type i) const
  {
    size_type res = this->samples[i / BLOCK_SIZE];
    for(size_type j = i - i % BLOCK_SIZE; j < i; j++)
    {
      if(this->small[j] >= LARGE_VALUE) { res++; }
    }
    return res;
  }

  sdsl::int_vector<8> small;
  sdsl::int_vector<0> large;
  sdsl::int_vector<0> samples;

//------------------------------------------------------------------------------

  class iterator :
    public std::iterator<std::random_access_iterator_tag, SLArray::value_type>
  {
  public:
    typedef SLArray::size_type size_type;

    iterator() : data(0), pos(0), rank(0) {}

    iterator(const SLArray* array, size_type offset) :
      data(array), pos(offset)
    {
      if(offset == 0) { this->rank = 0; }
      else if(offset >= array->size()) { this->rank = array->largeValues(); }
      else if(array->small[offset] == SLArray::LARGE_VALUE) { this->rank = array->large_rank(offset); }
      else { this->rank = UNKNOWN_RANK; }
    }

    size_type position() const { return this->pos; }

    iterator operator++ ()
    {
      if(this->data->small[this->pos] == SLArray::LARGE_VALUE) { this->rank++; }
      this->pos++;
      if(this->data->small[this->pos] == SLArray::LARGE_VALUE && this->rank == UNKNOWN_RANK)
      {
        this->rank = this->data->large_rank(this->pos);
      }
      return *this;
    }

    iterator operator-- ()
    {
      this->pos--;
      if(this->data->small[this->pos] == SLArray::LARGE_VALUE)
      {
        this->rank = (this->rank == UNKNOWN_RANK ? this->data->large_rank(this->pos) : this->rank - 1);
      }
      return *this;
    }

    iterator& operator+= (size_type n)
    {
      this->pos += n;
      this->rank = (this->data->small[this->pos] == SLArray::LARGE_VALUE ? this->data->large_rank(this->pos) : UNKNOWN_RANK);
      return *this;
    }

    iterator& operator-= (size_type n)
    {
      this->pos -= n;
      this->rank = (this->data->small[this->pos] == SLArray::LARGE_VALUE ? this->data->large_rank(this->pos) : UNKNOWN_RANK);
      return *this;
    }

    difference_type operator- (const iterator& another) const
    {
      return this->pos - another.pos;
    }

    iterator operator++ (int) { iterator res(*this); operator++(); return res; }
    iterator operator-- (int) { iterator res(*this); operator--(); return res; }
    iterator operator+ (size_type n) const { iterator res(*this); res += n; return res; }
    iterator operator- (size_type n) const { iterator res(*this); res -= n; return res; }

    bool operator== (const iterator& another) const { return (this->pos == another.pos); }
    bool operator!= (const iterator& another) const { return (this->pos != another.pos); }
    bool operator<  (const iterator& another) const { return (this->pos <  another.pos); }
    bool operator<= (const iterator& another) const { return (this->pos <= another.pos); }
    bool operator>= (const iterator& another) const { return (this->pos >= another.pos); }
    bool operator>  (const iterator& another) const { return (this->pos >  another.pos); }

    value_type operator* () const
    {
      if(this->data->small[this->pos] != SLArray::LARGE_VALUE) { return this->data->small[this->pos]; }
      else { return this->data->large[this->rank]; }
    }

    value_type operator[] (size_type n) const { return this->data->operator[](this->pos + n); }

  private:
    const SLArray* data;
    size_type      pos, rank;

    /*
      Computing the rank is expensive, and iterators often do not need it at all. Hence we
      determine the rank only when it is required.
    */
    const static size_type UNKNOWN_RANK = ~(size_type)0;
  };

  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, this->size()); }

//------------------------------------------------------------------------------

private:
  void copy(const SLArray& s);

  template<class IntVector>
  void
  buildFrom(IntVector& source)
  {
    sdsl::util::assign(this->small, sdsl::int_vector<8>(source.size() + 1, 0));

    size_type  large_values = 0;
    value_type max_large = 0;
    for(size_type i = 0; i < this->size(); i++)
    {
      if(source[i] < LARGE_VALUE) { this->small[i] = source[i]; }
      else
      {
        this->small[i] = LARGE_VALUE;
        large_values++; max_large = std::max(max_large, (value_type)(source[i]));
      }
    }

    if(large_values > 0)
    {
      size_type blocks = (this->size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
      sdsl::util::assign(this->large, sdsl::int_vector<0>(large_values, 0, bit_length(max_large)));
      sdsl::util::assign(this->samples, sdsl::int_vector<0>(blocks, 0, bit_length(large_values)));
      for(size_type block = 0, pos = 0, large_pos = 0; block < blocks; block++)
      {
        this->samples[block] = large_pos;
        size_type limit = std::min(this->size(), (block + 1) * BLOCK_SIZE);
        while(pos < limit)
        {
          if(source[pos] >= LARGE_VALUE)
          {
            this->large[large_pos] = source[pos]; large_pos++;
          }
          pos++;
        }
      }
    }
  }
};

}