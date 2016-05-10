#include <cassert>

#include <sstream>
#include <string>

#include <parse_rlzap.hpp>
#include <match.hpp>

#include <gtest/gtest.h>
#include "main.hpp"

class event_logger {
private:
  std::stringstream ss;
public:
  void copy(size_t source, size_t tgt, size_t len)
  {
    ss << "(" << source << ", " << tgt << ", " << len << ")";
  }

  void literal(size_t source, size_t len)
  {
    ss << "(" << source << ", " << len << ")";
  }

  std::string get_log()
  {
    return ss.str();
  }
};

TEST(RlzapParse, main)
{
  // Produce a dummy matching stats
  std::vector<rlz::match> matches {{
    rlz::match{100, 4},  // 0
    rlz::match{101, 3},  // 1 
    rlz::match{102, 2},  // 2
    rlz::match{103, 1},  // 3
    rlz::match{ 30, 6},  // 4
    rlz::match{ 31, 5},  // 5
    rlz::match{ 32, 4},  // 6
    rlz::match{ 33, 3},  // 7
    rlz::match{ 34, 2},  // 8
    rlz::match{ 35, 1},  // 9
    rlz::match{150, 5},  // 10
    rlz::match{151, 4},  // 11
    rlz::match{152, 3},  // 12
    rlz::match{153, 2},  // 13
    rlz::match{154, 1},  // 14
    rlz::match{ 40, 3},  // 15
    rlz::match{ 41, 2},  // 16
    rlz::match{ 42, 1},  // 17
    rlz::match{ 53, 8},  // 18
    rlz::match{ 54, 7},  // 19
    rlz::match{ 55, 6},  // 20
    rlz::match{ 56, 5},  // 21
    rlz::match{ 57, 4},  // 22
    rlz::match{ 58, 3},  // 23
    rlz::match{ 59, 2},  // 24
    rlz::match{ 60, 1},  // 25
  }};

  event_logger logger;
  auto copy_fun = [&] (size_t source, size_t tgt, size_t len) -> void {
    logger.copy(source, tgt, len);
  };

  auto lit_fun = [&] (size_t source, size_t len) -> void {
    logger.literal(source, len);
  };

  auto end_evt = [] () { };

  rlz::parser_rlzap{4, 16, 2}(matches.begin(), matches.end(), 155UL, lit_fun, copy_fun, end_evt);

  auto log        = logger.get_log();
  std::string exp = "(0, 4)(4, 30, 6)(10, 5)(15, 40, 3)(18, 53, 8)";
  ASSERT_EQ(exp, log);
}