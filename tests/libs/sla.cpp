#include "../include/sla.hpp"

namespace relative {

//------------------------------------------------------------------------------

SLArray::SLArray() :
  small(sdsl::int_vector<8>(1, 0))
{
}

SLArray::SLArray(const SLArray& s)
{
  this->copy(s);
}

SLArray::SLArray(SLArray&& s)
{
  *this = std::move(s);
}

SLArray::~SLArray()
{
}

SLArray::SLArray(sdsl::int_vector_buffer<0>& source)
{
  this->buildFrom(source);
}

void
SLArray::copy(const SLArray& s)
{
  this->small = s.small;
  this->large = s.large;
  this->samples = s.samples;
}

void
SLArray::swap(SLArray& s)
{
  if(this != &s)
  {
    this->small.swap(s.small);
    this->large.swap(s.large);
    this->samples.swap(s.samples);
  }
}

SLArray&
SLArray::operator=(const SLArray& s)
{
  if(this != &s) { this->copy(s); }
  return *this;
}

SLArray&
SLArray::operator=(SLArray&& s)
{
  if(this != &s)
  {
    this->small = std::move(s.small);
    this->large = std::move(s.large);
    this->samples = std::move(s.samples);
  }
  return *this;
}

SLArray::size_type
SLArray::serialize(std::ostream& out, sdsl::structure_tree_node* s, std::string name) const
{
  sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(s, name, sdsl::util::class_name(*this));
  SLArray::size_type written_bytes = 0;

  written_bytes += this->small.serialize(out, child, "small");
  written_bytes += this->large.serialize(out, child, "large");
  written_bytes += this->samples.serialize(out, child, "samples");

  sdsl::structure_tree::add_size(child, written_bytes);
  return written_bytes;
}

void
SLArray::load(std::istream& in)
{
  this->small.load(in);
  this->large.load(in);
  this->samples.load(in);
}

sdsl::int_vector<64>
SLArray::extract(SLArray::size_type from, SLArray::size_type to) const
{
  from = std::min(from, this->size()); to = std::min(to, this->size());

  sdsl::int_vector<64> result(to - from, 0);
  iterator iter(this, from);
  for(SLArray::size_type i = 0; i < result.size(); i++, ++iter) { result[i] = *iter; }

  return result;
}

}