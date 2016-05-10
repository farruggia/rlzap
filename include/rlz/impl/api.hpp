#pragma once

#include <memory>
#include <sstream>
#include <type_traits>
#include <stdexcept>
#include <vector>

#include "../build_coordinator.hpp"
#include "../containers.hpp"
#include "../get_matchings.hpp"
#include "../index.hpp"
#include "../io.hpp"
#include "../parse.hpp"
#include "../type_utils.hpp"

namespace rlz {

namespace impl {

template <typename Alphabet, typename ParseKeeper, typename LiteralKeeper, typename ParserType, typename Reference, typename Match_It>
index<Alphabet, Reference, ParseKeeper, LiteralKeeper> construct(
  const typename Alphabet::Symbol *input, Reference reference,
  ParserType parser,
  Match_It m_begin, Match_It m_end
)
{
  using Index = index<Alphabet, Reference, ParseKeeper, LiteralKeeper>;
  auto builder = std::make_shared<typename Index::builder>();
  std::vector<std::shared_ptr<build::observer<Alphabet>>> obs {{ builder }};
  build::coordinator<Alphabet> coord(obs.begin(), obs.end(), input);

  auto copy_evt    = [&] (size_t position, size_t ptr, size_t len) { return coord.copy_evt(position, ptr,len); };
  auto literal_evt = [&] (size_t position, size_t length) { return coord.literal_evt(position,length); };
  auto end_evt     = [&] () { return coord.end_evt(); };

  parser(m_begin, m_end, reference.size(), literal_evt, copy_evt, end_evt);
  return builder->get(std::move(reference));
}

template <typename Masked, typename Alphabet>
using Bind = typename Masked::template Type<Alphabet>;

template <typename Alphabet, typename Reference, typename ParseKeeper, typename LiteralKeeper>
using Index = index<Alphabet, Reference, Bind<ParseKeeper, Alphabet>, Bind<LiteralKeeper, Alphabet>>;

template <typename ContainerAlphabet, typename RequestedAlphabet, typename Container>
struct returner {

  returner(Container v) { }

  using Type = iterator_container<RequestedAlphabet, typename RequestedAlphabet::Symbol*>;

  Type get()
  {
    throw std::logic_error("Reference/Stream has wrong underlying alphabet type");
  }
};

template <typename Alphabet, typename Container>
struct returner<Alphabet, Alphabet, Container> {
  Container v;
  returner(Container v) : v(v) { }

  using Type = Container;
  Type get() { return std::move(v); }
};

template <typename Alphabet, typename Reference>
class getter_factory {
  using WrappedReference = container_wrapper<Alphabet, Reference>;
  WrappedReference ref;
public:

  template <typename U>
getter_factory(U &&ref) : ref(std::forward<U>(ref)) { }

  template <typename GotAlphabet>
  typename returner<Alphabet, GotAlphabet, WrappedReference>::Type get() const
  {
    if (!std::is_same<Alphabet, GotAlphabet>::value) {
      throw std::logic_error("getter_factory: wrong alphabet");
    }    
    return returner<Alphabet, GotAlphabet, WrappedReference>(std::move(ref)).get();
  }
};

template <typename Alphabet, typename Iterator>
class iterator_factory {
  using Container = iterator_container<Alphabet, Iterator>;
  Container container;
public:

  iterator_factory() { }
  iterator_factory(Iterator begin, Iterator end) : container(begin, end) { }

  template <typename GotAlphabet>
  typename returner<Alphabet, GotAlphabet, Container>::Type get() const
  {
    if (!std::is_same<Alphabet, GotAlphabet>::value) {
      throw std::logic_error("iterator_factory: wrong alphabet");
    }
    return returner<Alphabet, GotAlphabet, Container>(container).get();
  }
};

class stream_factory {
  std::istream &reference;
public:
  stream_factory(std::istream &reference) : reference(reference) { }

  template <typename Alphabet>
  using Result = iterator_container<Alphabet, typename Alphabet::Symbol*, shared_ownership<typename Alphabet::Symbol>>;

  template <typename Alphabet>
  Result<Alphabet> get()
  {
    using Symbol = typename Alphabet::Symbol;
    size_t length;
    auto data_unique = io::read_stream<Symbol>(reference, &length);
    shared_ownership<Symbol> owner {std::move(data_unique)};
    return Result<Alphabet>(
      owner.get(), owner.get() + length, owner
    );
  }
};

}
}