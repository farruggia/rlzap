#include <alphabet.hpp>
#include <sa_compute.hpp>

#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <io.hpp>

std::string file_prefix = "";

template <typename Alphabet>
struct suffix { };

template <>
struct suffix<rlz::alphabet::dna<>> {
  std::string operator()()
  {
    return "dna";
  }
};

template <>
struct suffix<rlz::alphabet::dlcp_32> {
  std::string operator()()
  {
    return "int32";
  }
};

template <>
struct suffix<rlz::alphabet::lcp_32> {
  std::string operator()()
  {
    return "uint32";
  }
};

template <typename Alphabet>
class SaGetter : public ::testing::Test {
public:
  using Symbol    = typename Alphabet::Symbol;

  const size_t to_gen = 100000;

  std::vector<Symbol> get_vector()
  {
    std::vector<Symbol> to_ret;
    if (file_prefix == "") {
      const auto min = Symbol{} + 1;
      const auto max = std::numeric_limits<Symbol>::max() - 1;
      std::mt19937 gen(42);
      std::uniform_int_distribution<Symbol> dis(min, max);
      for (auto i = 0U; i < to_gen - 1; ++i) {
        to_ret.push_back(dis(gen));
      }
      to_ret.push_back(0);
      return to_ret;      
    }
    std::stringstream ss;
    ss << file_prefix << "_" << suffix<Alphabet>{}();
    auto input_filename = ss.str();
    std::ifstream file(input_filename.c_str(), std::ios_base::binary);
    auto len = rlz::io::stream_length(file) / sizeof(Symbol);
    to_ret.resize(len);
    if (!file.read(reinterpret_cast<char*>(to_ret.data()), to_ret.size() * sizeof(Symbol))) {
      std::stringstream ss;
      ss << "ERROR: cannot read file " << input_filename;
      throw std::logic_error(ss.str());
    }
    return to_ret;
  }
};

using Alphabets = ::testing::Types<
  rlz::alphabet::dna<>,
  rlz::alphabet::dlcp_32,
  rlz::alphabet::lcp_32
>;

TYPED_TEST_CASE(SaGetter, Alphabets);

template <typename Cont>
std::string print_suffix(const Cont &t, size_t start)
{
  std::stringstream ss;
  for (auto i = start; i < t.size(); ++i) {
    unsigned int to_print = t[i];
    ss << to_print << " ";
  }
  return ss.str();
}

template <typename T>
::testing::AssertionResult smaller_suffix(
  const char *exp_1, const char *exp_2, const char *exp_3,
  std::vector<T> &vec, size_t left, size_t right
) {

  for (; (left < vec.size()) and (right < vec.size()); ++left, ++right) {
    if (vec[left] > vec[right]) {
      return ::testing::AssertionFailure()
                << "Smaller suffix: left suffix greater than right.\n"
                << "LP = " << left << "\n"
                << "RP = " << right << "\n"
                << "LS = " << print_suffix(vec, left) << "\n"
                << "RS = " << print_suffix(vec, right);
    } else if (vec[left] < vec[right]) {
      return ::testing::AssertionSuccess();
    }
  }
  if (left < right) {
      return ::testing::AssertionFailure()
                << "Smaller suffix: left suffix greater than right.\n"
                << "LP = " << left << "\n"
                << "RP = " << right << "\n"
                << "LS = " << print_suffix(vec, left) << "\n"
                << "RS = " << print_suffix(vec, right);
  }
  return ::testing::AssertionSuccess();
}

template <typename T>
::testing::AssertionResult equal_suffix(
  const char *exp_1, const char *exp_2, const char *exp_3, const char *exp_4,
  std::vector<T> &vec, size_t left, size_t right, size_t n
) {
  if (left + n > vec.size()) {
    return ::testing::AssertionFailure()
              << "Equal suffix: left overflow"
              << "( left = " << left << ", n = " << n << ", size = " << vec.size();
  }
  if (right + n > vec.size()) {
    return ::testing::AssertionFailure()
              << "Equal suffix: right overflow"
              << "( right = " << right << ", n = " << n << ", size = " << vec.size();
  }

  for (auto i = 0U; i < n; ++i) {
    if (vec[left + i] != vec[right + i]) {
      return ::testing::AssertionFailure()
                << "Equal suffix: vec[" << left << " + " << i << "] != vec["<< right << " + " << i << "]" << "\n"
                << "LEN = " << vec.size() << "\n"
                << "N   = " << n << "\n"
                << "L   = " << left << "\n"
                << "R   = " << right << "\n"
                << "VL  = " << print_suffix(vec, left) << "\n"
                << "VR  = " << print_suffix(vec, right);
    }
  }
  return ::testing::AssertionSuccess();
}

TYPED_TEST(SaGetter, ComputeSA)
{
  using Symbol = typename TypeParam::Symbol;
  auto vec = this->get_vector();
  sdsl::int_vector<> sa, lcp;
  sdsl::memory_monitor::start();
  rlz::sa_compute<Symbol>{}(vec.data(), sa, lcp, vec.size());
  sdsl::memory_monitor::stop();
  if (file_prefix != "") {
    std::cout << "--- Peak memory usage = " << sdsl::memory_monitor::peak() / (1024*1024) << " MB" << std::endl;
  }
  // DEBUG: print info
  // for (auto i = 0; i < vec.size(); ++i) {
  //   std::cout << lcp[i] << "\t" << print_suffix(vec, sa[i]) << std::endl;
  // }

  // Check sizes
  ASSERT_GE(sa.size(), vec.size());
  ASSERT_GE(lcp.size(), vec.size() - 1);

  // Check SA
  for (auto i = 0U; i + 1 < sa.size(); ++i) {
    auto left = sa[i], right = sa[i + 1];
    ASSERT_PRED_FORMAT3(smaller_suffix, vec, left, right);
  }

  // Check LCP
  for (auto i = 1U; i < sa.size(); ++i) {
    auto left   = sa[i], right = sa[i - 1];
    auto t_lcp  = lcp[i];
    ASSERT_PRED_FORMAT4(equal_suffix, vec, left, right, t_lcp);
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  if (argc > 1) {
    file_prefix = *++argv;
  }
  return RUN_ALL_TESTS();
}