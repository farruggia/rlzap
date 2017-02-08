#pragma once

#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

#include <sdsl/bit_vectors.hpp>
#include <sdsl/rank_support_v5.hpp>
#include <sdsl/util.hpp>

#include <boost/iterator/filter_iterator.hpp>

#include "bitsets.hpp"
#include "impl/bit_vectors.hpp"
#include "sdsl_extensions/sd_vector.hpp"
#include "type_name.hpp"
#include "type_utils.hpp"


namespace rlz { namespace vectors {
//////////////////////////////////// BASE /////////////////////////////////////
template <typename bv_type>
class base {
protected:
  bv_type bv;

  template <typename BvType>
  base(BvType &&bv_) : bv(std::forward<BvType>(bv_)) { }
  base() { }

  template <typename R>
  void set_bv(R &&r)
  {
    std::swap(bv, std::forward<R>(r));
  }
public:
  using reference       = decltype(bv[0]);
  using value_type      = typename bv_type::value_type;
  using vector_type     = bv_type;
  using iterator        = decltype(bv.begin());
  
  reference  operator[](const size_t &idx)       { return bv[idx]; }
  value_type operator[](const size_t &idx) const { return bv[idx]; }

  std::uint64_t get_int(size_t idx, const std::uint8_t len = 64) const { return bv.get_int(idx, len); }

  auto begin() const -> decltype(bv.begin()) { return bv.begin(); }
  auto end() const -> decltype(bv.end()) { return bv.end(); }

  size_t size() const { return bv.size(); }
  const bv_type &data() const { return bv; }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="")const
  {
    return bv.serialize(out, v, "");
  }
  void load(std::istream& in) { bv.load(in); }
};

/////////////////////////////////// BUILDERS //////////////////////////////////
namespace builders {

class sparse {
private:
  std::vector<unsigned int> sets;
  using sets_it = std::vector<unsigned int>::iterator;
  const size_t len;
public:

  sparse(size_t len) : len(len) { }
  void set(size_t position)
  {
    assert(position < len);
    assert(sets.empty() or position > sets.back());
    sets.push_back(position);
  }

  std::tuple<sets_it, sets_it, size_t> get() { return std::make_tuple(sets.begin(), sets.end(), len); }
};

class dense {
private:
  sdsl::bit_vector bv;
public:

  dense(size_t len) : bv(len, 0) { }

  void set(size_t position)
  {
    assert(position < bv.size());
    bv[position] = 1;
  }

  sdsl::bit_vector get()
  {
    sdsl::bit_vector to_ret;
    std::swap(to_ret, bv);
    return to_ret;
  }
};
}

///////////////////////////// BASIC VECTORS ///////////////////////////////////
class sparse : public base<sdsl::extensions::sd_vector<>> {
private:
  using Rep  = sdsl::extensions::sd_vector<>;
  using Base = base<Rep>;

  template <typename It>
  Rep build_sparse(const std::tuple<It, It, size_t> &pack)
  {
    It begin, end;
    size_t size;
    std::tie(begin, end, size) = pack;
    return Rep(begin, end, size);
  }

  template <typename BitVector>
  Rep build_bv(BitVector &&bv) { return Rep(bv); }

public:
  using builder       = builders::sparse;

  template <typename It>
  sparse(const std::tuple<It, It, size_t> &pack) : Base(build_sparse(pack)) { }

  // Bitvector (SFINAE to select this only when we pass a BitVector)
  template <typename BitVector, typename = rlz::impl::if_same<BitVector, sdsl::bit_vector>>
  sparse(BitVector &&bv) : Base(build_bv(std::forward<BitVector>(bv))) { }

  sparse() { }
  sparse(const sparse &other) = default;
  sparse(sparse &&other) = default;
  sparse &operator=(const sparse &other) = default;
  sparse &operator=(sparse &&other) = default;

  template <typename R>
  void set(R &&r)
  {
    sparse temp(r.get());
    std::swap(*this, temp);
  }
};  

class dense : public base<sdsl::bit_vector>
{
private:
  template <typename It>
  sdsl::bit_vector build(const std::tuple<It, It, size_t> &pack)
  {
    It begin, end;
    size_t len;
    std::tie(begin, end, len) = pack;
    sdsl::bit_vector bv(len, 0);
    for (auto it = begin; it != end; ++it) {
      bv[*it] = 1;
    }
    return bv;
  }
public:

  using builder = builders::dense;

  template <typename It>
  dense(const std::tuple<It, It, size_t> &pack) : base<sdsl::bit_vector>(build(pack)) { }

  // Bitvector (SFINAE to select this only when we pass a BitVector)
  template <typename BitVector, typename = rlz::impl::if_same<BitVector, sdsl::bit_vector>>
  dense(BitVector &&bv) : base<sdsl::bit_vector>(std::forward<BitVector>(bv)) { }

  dense() { }
  dense(const dense &other) = default;
  dense(dense &&other) = default;
  dense &operator=(const dense &other) = default;
  dense &operator=(dense &&other) = default;

  template <typename R>
  void set(R &&r)
  {
    dense temp(r.get());
    std::swap(*this, temp);
  }

};

////////////////////////////////// ALGORITHMS /////////////////////////////////
namespace algorithms {

template <typename rank_type>
class rank {
protected:
  rank_type bv_rank;

  template <typename Bv_Type>
  void init(Bv_Type *bv_) { sdsl::util::init_support(bv_rank, &(bv_->data())); }

  template <typename Bv_Type>
  void copy(Bv_Type *bv_) { init(bv_); }

public:

  size_t rank_1(const size_t &idx) const { return idx == 0 ? 0 : bv_rank(idx); }
  size_t rank_0(const size_t &idx) const { return idx - rank_1(idx); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
      sdsl::structure_tree_node* child = sdsl::structure_tree::add_child(v, name, "rank");
      size_t written_bytes = 0;
      written_bytes += bv_rank.serialize(out, child, "ds");
      sdsl::structure_tree::add_size(child, written_bytes);
      return written_bytes;
  }

  template <typename Bv_Type>
  void load(std::istream& in, Bv_Type *bv) { bv_rank.load(in, &(bv->data())); }
};

template <typename select_type>
class select {
protected:
  select_type bv_select;

  template <typename Bv_Type>
  void init(Bv_Type *bv_) { sdsl::util::init_support(bv_select, &(bv_->data())); }

  template <typename Bv_Type>
  void copy(Bv_Type *bv_) { init(bv_); }

  size_t inner_serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
      return bv_select.serialize(out, v, "ds");
  }

public:

  size_t select_1(const size_t &idx) const { return bv_select(idx); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
      auto child         = sdsl::structure_tree::add_child(v, name, "select");
      auto written_bytes = inner_serialize(out, child, "ds");
      sdsl::structure_tree::add_size(child, written_bytes);
      return written_bytes;
  }

  template <typename Bv_Type>
  void load(std::istream& in, Bv_Type *bv) { bv_select.load(in, &(bv->data())); }
};

template <typename BsType>
class bitset {
protected:
  BsType bs;

  template <typename Bv_Type>
  void init(Bv_Type *bv_) { bs.init(bv_); }

  template <typename Bv_Type>
  void copy(Bv_Type *bv_) { bs.copy(bv_); }

public:

  typename BsType::iterator get_bitset(size_t index) const { return bs(index); }
  typename BsType::iterator get_bitset_end()         const { return bs(); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return bs.serialize(out, v, name);
  }

  template <typename Bv_Type>
  void load(std::istream& in, Bv_Type *bv) { bs.load(in, bv); }
};

enum class Kind { algorithm, service };

template <template <typename...> class V>
struct GetKind { };

template <>
struct GetKind<rank> {
  static constexpr Kind get() { return Kind::algorithm; }
};

template <>
struct GetKind<select> {
  static constexpr Kind get() { return Kind::algorithm; }
};

template <>
struct GetKind<bitset> {
  static constexpr Kind get() { return Kind::service; }
};

}

///////////////////////////////// MIX-IN BITVECTOR ////////////////////////////
template <typename Rep, typename... Mixins>
class bit_vector;

template <typename Rep>
class bit_vector<Rep> : public Rep {
protected:
  size_t inner_serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    return Rep::serialize(out, v, name);
  }
public:

  bit_vector() { }

  template <typename Init>
  bit_vector(Init &&r) : Rep(std::forward<Init>(r)) { }

    // Initialize with sparse vector
  template <typename It>
  bit_vector(It begin, It end, size_t len) : Rep(std::make_tuple(begin, end, len)) { }

  // Sets a Builder
  template <typename R>
  void set(R &&r) { Rep::set(std::forward<R>(r)); }

  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    auto child          = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    auto written_bytes  = Rep::serialize(out, child, name);
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }
};

template <typename Rep, typename Op_1, typename... Ops>
class bit_vector<Rep, Op_1, Ops...> : public bit_vector<Rep, Ops...>, public Op_1
{
private:
  void set_op() { Op_1::init(this); }

  using Base = bit_vector<Rep, Ops...>;

protected:

  size_t inner_serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    auto written_bytes = 0UL;
    written_bytes += bit_vector<Rep, Ops...>::inner_serialize(out, v, name);
    written_bytes += Op_1::serialize(out, v, name);
    return written_bytes;
  }

public:

   // Empty bit_vector
  bit_vector() { set_op(); }

  // Initialize with whatever single-argument Rep supports (such as bit-vectors)
  template <typename R>
  bit_vector(R &&r) : Base(std::forward<R>(r)) { set_op(); }

  // Default copy/move constructor
  bit_vector(const bit_vector<Rep, Op_1, Ops...> &bv) 
    : Base(bv), Op_1(bv)
  { 
    Op_1::copy(this); 
  }

  bit_vector(bit_vector<Rep, Op_1, Ops...> &&bv) 
    : Base(std::move(bv)), Op_1(std::move(bv))
  { 
    Op_1::copy(this); 
  }

  // Initialize with sparse vector
  template <typename It>
  bit_vector(It begin, It end, size_t len) : Base(begin, end, len) { set_op(); }

  // Copy/Move assignment operators (cannot be template!)
  bit_vector &operator=(const bit_vector &other)
  { 
    Base::operator=(other);
    Op_1::operator=(other);
    Op_1::copy(this);
    return *this; 
  }
  bit_vector &operator=(bit_vector &&other)
  {
    Base::operator=(std::move(other));
    Op_1::operator=(std::move(other));
    Op_1::copy(this);
    return *this;
  }

  // Serialization functions
  size_t serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="") const
  {
    auto child          = sdsl::structure_tree::add_child(v, name, rlz::util::type_name(*this));
    auto written_bytes  = inner_serialize(out, child, name);
    sdsl::structure_tree::add_size(child, written_bytes);
    return written_bytes;
  }

  void load(std::istream& in)
  {
    bit_vector<Rep, Ops...>::load(in);
    Op_1::load(in, this);
  }

  // Sets a Builder
  template <typename R>
  void set(R &&r)
  {
    bit_vector<Rep, Ops...>::set(std::forward<R>(r));
    set_op();
  }
};

/////////////////////////////// MIX-IN TYPE HANDLING ///////////////////////////
namespace impl {

using type_utils::type_list;
using type_utils::join_lists;
using type_utils::Apply;

////////////////////////////////// MIX-IN SORTER //////////////////////////////
template <template <typename...> class T, algorithms::Kind kind>
using EnableKind = typename std::enable_if<algorithms::GetKind<T>::get() == kind, void>::type;

template <template <typename...> class... L>
struct generic_list { };

template <
  template <template <typename...> class...> class T, 
  typename Unsorted, 
  typename Service, 
  typename Algorithms, 
  typename = void
>
struct sort_types { };

template <
  template <template <typename...> class...> class T,
  template <typename...> class... Services, 
  template <typename...> class... Algorithms
>
struct sort_types<T, 
        generic_list<>,
        generic_list<Services...>,
        generic_list<Algorithms...>
       >
{
  using type = T<Services..., Algorithms...>;
};

template <
  template <template <typename...> class...> class T, 
  template <typename...> class    UService, 
  template <typename...> class... UL, 
  template <typename...> class... Services, 
  template <typename...> class... Algorithms
>
struct sort_types<T, 
        generic_list<UService, UL...>, 
        generic_list<Services...>, 
        generic_list<Algorithms...>, 
        EnableKind<UService, algorithms::Kind::service>
       >
{
  using type = typename sort_types<T, 
                          generic_list<UL...>, 
                          generic_list<UService, Services...>, 
                          generic_list<Algorithms...>
                        >::type;
};

template <
  template <template <typename...> class...> class T, 
  template <typename...> class    UAlgorithm, 
  template <typename...> class... UL, 
  template <typename...> class... Services, 
  template <typename...> class... Algorithms
>
struct sort_types<T, 
        generic_list<UAlgorithm, UL...>, 
        generic_list<Services...>, 
        generic_list<Algorithms...>, 
        EnableKind<UAlgorithm, algorithms::Kind::algorithm>
       >
{
  using type = typename sort_types<T, 
                          generic_list<UL...>, 
                          generic_list<Services...>, 
                          generic_list<UAlgorithm, Algorithms...>
                        >::type;
};

template <template <template <typename...> class...> class T, typename List>
using SortTypes = typename sort_types<T, List, generic_list<>, generic_list<>>::type;

//////////////////////////////// MIX-IN INSTANTIATE ///////////////////////////
template <template <typename...> class Algorithm, typename Bv>
struct instantiate_algo;

template <typename... L>
struct instantiate_algo<algorithms::rank, bit_vector<sparse, L...>>
{
  using type = algorithms::rank<sdsl::extensions::rank_support_sd<>>;
};

template <typename... L>
struct instantiate_algo<algorithms::rank, bit_vector<dense, L...>>
{
  using type = algorithms::rank<sdsl::rank_support_v5<>>;
};

template <typename... L>
struct instantiate_algo<algorithms::select, bit_vector<sparse, L...>>
{
  using type = algorithms::select<sdsl::extensions::select_support_sd<>>;
};

template <typename... L>
struct instantiate_algo<algorithms::select, bit_vector<dense, L...>>
{
  using type = algorithms::select<sdsl::select_support_mcl<>>;
};

template <typename... L>
struct instantiate_algo<algorithms::bitset, bit_vector<sparse, L...>>
{
  using type = algorithms::bitset<algorithms::bitsets::sparse<bit_vector<sparse, L...>>>;
};

template <typename... L>
struct instantiate_algo<algorithms::bitset, bit_vector<dense, L...>>
{
  using type = algorithms::bitset<algorithms::bitsets::dense<bit_vector<dense, L...>>>;
};

template <template <typename...> class Algorithm, typename Bv>
using InstantiateAlgo = typename instantiate_algo<Algorithm, Bv>::type;

///////////////////////////// MIX-IN LISTS INSTANTIATE ////////////////////////
template <typename Rep, typename Algos>
struct instantiate_algos { };

template <typename Rep>
struct instantiate_algos<Rep, generic_list<>> {
  using type = type_list<>;
};

template <typename Rep, template <typename...> class O_1, template <typename...> class... O_Rest>
struct instantiate_algos<Rep, generic_list<O_1, O_Rest...>> {
  using Rest   = typename instantiate_algos<Rep, generic_list<O_Rest...>>::type;
  using RestBV = typename Apply<bit_vector, typename join_lists<type_list<Rep>, Rest>::type>::type;
  using Head   = type_list<InstantiateAlgo<O_1, RestBV>>;
  using type   = typename join_lists<Head, Rest>::type;
};
}


//////////////////////////////// TYPE GETTER //////////////////////////////////
template <typename Rep, template <typename> class... U>
using BitVector =                            // A BitVector is...
  typename impl::Apply<                      // an instantiation of T with typelist L
    bit_vector,                              // where T = bit_vector (which takes a rep and list of ops)
    typename impl::join_lists<               // and L is the union of...
      impl::type_list<Rep>,                  // <Rep>, and...
      typename impl::instantiate_algos<Rep,  // each instantiation of the algorithms...
        impl::SortTypes<impl::generic_list,  // ...sorted by kind.
          impl::generic_list<U...>>
      >::type
    >::type
  >::type;
}}