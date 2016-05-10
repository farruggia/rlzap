#include <string>
#include <sstream>

#include <dna_packer.hpp>
#include <integer_type.hpp>

#include "main.hpp"

std::string junk = "GGTNNNAANNTCAGGTAACGTTTANNATAAATCTCAANNNNNNNNNNNNN";

struct instantiate {
  template <typename BlockSize>
  rlz::dna_pack<BlockSize> get()
  {
    return rlz::dna_pack<BlockSize>(junk.begin(), junk.end());
  }
};

struct serialize {
  template <typename BlockSize>
  rlz::dna_pack<BlockSize> get()
  {
    auto pack = instantiate{}.get<BlockSize>();
    std::stringstream output;
    pack.serialize(output);
    std::stringstream input(output.str());
    rlz::dna_pack<BlockSize> to_ret;
    to_ret.load(input);
    return to_ret;
  }
};

template <typename BlockSize, typename Get>
struct param {
  using Size   = BlockSize;
  using Getter = Get;
};

template <typename Param>
class dna_packer : public ::testing::Test {
  using BlockSize = typename Param::Size;
  using Getter    = typename Param::Getter;
public:
  void SetUp() override
  {
	// Do nothing
  }

  rlz::dna_pack<BlockSize> get()
  {
    return Getter{}.template get<BlockSize>();
  }
};

using Types = ::testing::Types<
  param<rlz::values::Size<8>, instantiate>,
  param<rlz::values::Size<16>, instantiate>,
  param<rlz::values::Size<32>, instantiate>,
  param<rlz::values::Size<8>, serialize>,
  param<rlz::values::Size<16>, serialize>,
  param<rlz::values::Size<32>, serialize>
>;

TYPED_TEST_CASE(dna_packer, Types);

TYPED_TEST(dna_packer, Length)
{
  auto pack = this->get();
  ASSERT_EQ(junk.size(), pack.size());
}

TYPED_TEST(dna_packer, Access)
{
  auto pack = this->get();
  for (auto i = 0U; i < junk.size(); ++i) {
	ASSERT_EQ(junk[i], pack[i]);
  }
}

TYPED_TEST(dna_packer, FullIterator)
{
  auto pack    = this->get();
  auto junk_it = junk.begin();
  auto pack_it = pack.begin();

  while (junk_it != junk.end()) {
	ASSERT_EQ(*junk_it++, *pack_it++);
  }
  ASSERT_EQ(pack_it, pack.end());
}

TYPED_TEST(dna_packer, IteratorSkip)
{
  auto pack    = this->get();
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
