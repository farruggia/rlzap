#include <cstdint>
#include <string>
#include <sstream>
#include <vector>

#include <lcp_pack.hpp>
#include <integer_type.hpp>

#include "main.hpp"

template <typename T, bool sgd>
struct getter {
};

template <typename T>
struct getter<T, true>
{
    static std::vector<T> get()
    {
        return {{
          -100,
          20,
          -10,
          0,
          std::numeric_limits<T>::min(),
          std::numeric_limits<T>::max(),
          -129,
          -128,
          -127,
          125,
          126,
          127,
          128,
          255,
          256
        }};
    };
};

template <typename T>
struct getter<T, false>
{
    static std::vector<T> get()
    {
        return {{
          0U,
          20U,
          254U,
          255U,
          256U,
          256U,
          255U,
          0U,
          std::numeric_limits<T>::min(),
          std::numeric_limits<T>::max(),
          0U,
          128U,
          127U,
          126U,
          254U,
          256U,
          255U
        }};
    };
};

template <typename T>
struct junk_hold {
  static std::vector<T> get()
  {
      return getter<T, std::is_signed<T>::value>::get();
  }
};

struct instantiate {
  template <typename Symbol>
  rlz::lcp_pack<Symbol> get()
  {
    auto v = junk_hold<Symbol>::get();
    return rlz::lcp_pack<Symbol>(v.begin(), v.end());
  }
};

struct serialize {
  template <typename Symbol>
  rlz::lcp_pack<Symbol> get()
  {
    auto pack = instantiate{}.get<Symbol>();
    std::stringstream output;
    pack.serialize(output);
    std::stringstream input(output.str());
    rlz::lcp_pack<Symbol> to_ret;
    to_ret.load(input);
    return to_ret;
  }
};

template <typename Symbol, typename Get>
struct param {
  using Size   = Symbol;
  using Getter = Get;
};

template <typename Param>
class lcp_packer : public ::testing::Test {
  using Symbol = typename Param::Size;
  using Getter    = typename Param::Getter;
public:
  void SetUp() override
  {
	// Do nothing
  }

  std::vector<Symbol> get_junk()
  {
    return junk_hold<Symbol>::get();
  }

  rlz::lcp_pack<Symbol> get()
  {
    return Getter{}.template get<Symbol>();
  }
};

using Types = ::testing::Types<
  param<std::int16_t, instantiate>,
  param<std::int32_t, instantiate>,
  param<std::uint32_t, instantiate>,
  param<std::int16_t, serialize>,
  param<std::int32_t, serialize>,
  param<std::uint32_t, serialize>
>;

TYPED_TEST_CASE(lcp_packer, Types);

TYPED_TEST(lcp_packer, Length)
{
  auto pack = this->get();
  auto junk = this->get_junk();
  ASSERT_EQ(junk.size(), pack.size());
}

TYPED_TEST(lcp_packer, Access)
{
  auto pack = this->get();
  auto junk = this->get_junk();
  for (auto i = 0U; i < junk.size(); ++i) {
	ASSERT_EQ(junk[i], pack[i]);
  }
}

TYPED_TEST(lcp_packer, FullIterator)
{
  auto pack    = this->get();
  auto junk = this->get_junk();
  auto junk_it = junk.begin();
  auto pack_it = pack.begin();

  while (junk_it != junk.end()) {
	ASSERT_EQ(*junk_it++, *pack_it++);
  }
  ASSERT_EQ(pack_it, pack.end());
}

TYPED_TEST(lcp_packer, IteratorSkip)
{
  auto pack    = this->get();
  auto junk = this->get_junk();
  auto junk_it = junk.begin();
  auto pack_it = pack.begin();

  auto pow      = 0U;
  auto tot_skip = 0U;
  while (true) {
	auto skip = 1 << pow++;
	tot_skip += skip;
	if (tot_skip < pack.size()) {
	  std::advance(junk_it, skip);
	  std::advance(pack_it, skip);
	  ASSERT_EQ(*junk_it, *pack_it);
	} else {
		break;
	}
  }
}
