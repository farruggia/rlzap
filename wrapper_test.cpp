#include <iterator>
#include <iostream>
#include <type_traits>
#include <vector>

#include <boost/iterator/iterator_facade.hpp>

#include <alphabet.hpp>
#include <dumper.hpp>
#include <get_matchings.hpp>

#include <api.hpp>
#include <parse_rlzap.hpp>

namespace rlz {

namespace detail {
  template <typename T>
  using SignedType = typename std::make_signed<typename std::remove_reference<T>::type>::type;
}

template <
  typename RandomIt,
  typename Value = detail::SignedType<decltype(*RandomIt{})>
>
class differential_iterator : public boost::iterator_facade<
                                differential_iterator<RandomIt>,
                                Value const,
                                boost::forward_traversal_tag,
                                Value const
                              >
{
  Value previous;
  RandomIt it;
  friend class boost::iterator_core_access;
public:
  differential_iterator(RandomIt it) : previous(Value{}), it(it) { }

  void increment()
  {
    previous = *it;
    std::advance(it, 1); 
  }

  bool equal(differential_iterator<RandomIt, Value> const& other) const
  {
      return this->it == other.it;
  }

  Value dereference() const
  {
    return *it - previous;
  }
};

class wrapped_index {
private:
public:
  wrapped_index() { }

};

template <typename RefIt, typename InputIt>
wrapped_index get_index(RefIt ref_beg, RefIt ref_end, InputIt input_beg, InputIt input_end)
{
  // Step 1: parsing + index

  using SignedAlphabet   = rlz::alphabet::dlcp_32;
  using UnsignedAlphabet = rlz::alphabet::lcp_32;
  using T                = typename SignedAlphabet::Symbol;
  using DiffIt           = differential_iterator<RefIt, T>;

  rlz::utils::iterator_dumper<DiffIt> ref_dumper{DiffIt(ref_beg), DiffIt(ref_end)},
                                      input_dumper{DiffIt(input_beg), DiffIt(input_end)};

  auto matches = std::get<0>(
    rlz::get_relative_matches<SignedAlphabet>(ref_dumper, input_dumper)
  );


  std::cout << "Ref: ";
  for (auto it = ref_beg; it != ref_end; ++it) {
    std::cout << std::setw(3) << *it << " ";
  }
  std::cout << "\n-----" << std::endl;
  std::cout << "Dif: ";
  for (auto it = DiffIt(ref_beg); it != DiffIt(ref_end); ++it) {
    std::cout << std::setw(3) << *it << " ";
  }
  std::cout << "\n=====" << std::endl;

  std::cout << "Inp: ";
  for (auto it = input_beg; it != input_end; ++it) {
    std::cout << std::setw(3) << *it << " ";
  }
  std::cout << "\n-----" << std::endl;
  std::cout << "Dif: ";
  for (auto it = DiffIt(input_beg); it != DiffIt(input_end); ++it) {
    std::cout << std::setw(3) << *it << " ";
  }
  std::cout << "\n=====" << std::endl;

  auto pos = 0UL;
  for (const auto &m : matches) {
    std::cout << pos++ << " <- " << m.ptr << " @ " << m.len << std::endl;
  }

  // Get index builder. Prerequisites: source (reference) container.
  using SourceContainer = rlz::iterator_container<UnsignedAlphabet, RefIt>;
  using Index           = rlz::index<UnsignedAlphabet, SourceContainer>;
  using IndexBuilder    = typename Index::template builder<RefIt>;

  using PK              = rlz::impl::Bind<rlz::api::ParseKeeper<>, UnsignedAlphabet>;
  using LK              = rlz::impl::Bind<rlz::api::LiteralKeeper<>, UnsignedAlphabet>;

  const size_t delta_pointer_size = 4UL;
  const size_t abs_pointer_size   = 32UL;
  const size_t symbol_length      = 32UL;

  SourceContainer reference{ref_beg, ref_end};
  rlz::parser_rlzap parser{delta_pointer_size, abs_pointer_size, symbol_length};

  auto index = rlz::impl::construct<UnsignedAlphabet, PK, LK>(
    input_beg,
    reference,
    parser,
    matches.begin(), matches.end()
  );

  auto index_size = index.size();
  std::cout << "Index size: " << index_size << std::endl;
  for (auto i = 0U; i < index_size; ++i) {
    std::cout << std::setw(3) << index(i) << " ";
  }
  std::cout << std::endl;

  return wrapped_index{};
}
  
}


int main(int argc, char **argv)
{
  std::vector<std::uint32_t> ref_lcps {{ 1, 5, 3, 0, 7, 2 }};
  std::vector<std::uint32_t> input_lcps {{ 1, 5, 8, 6, 13, 8 }};
  auto W = rlz::get_index(
    ref_lcps.begin(), ref_lcps.end(),
    input_lcps.begin(), input_lcps.end()
  );
}