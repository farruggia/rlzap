#include <sstream>
#include <string>

#include <classic_parse.hpp>
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

TEST(ClassicParse, main)
{
  // Produce a dummy matching stats
  std::vector<rlz::match> matches {{
    rlz::match{0,0},  // 0
    rlz::match{4,5},  // 1 
    rlz::match{5,4},  // 2
    rlz::match{6,3},  // 3
    rlz::match{7,2},  // 4
    rlz::match{8,1},  // 5
    rlz::match{0,7},  // 6
    rlz::match{1,6},  // 7
    rlz::match{2,5},  // 8
    rlz::match{3,4},  // 9
    rlz::match{4,3},  // 10
    rlz::match{5,2},  // 11
    rlz::match{6,1},  // 12
    rlz::match{0,0},  // 13
    rlz::match{0,0},  // 14
    rlz::match{0,0},  // 15
    rlz::match{3,1}   // 16
  }};

  event_logger logger;
  auto copy_fun = [&] (size_t source, size_t tgt, size_t len) -> void {
    logger.copy(source, tgt, len);
  };

  auto lit_fun = [&] (size_t source, size_t len) -> void {
    logger.literal(source, len);
  };

  auto end_evt = [] () { };

  rlz::classic::parser{}(matches.begin(), matches.end(), 9UL, lit_fun, copy_fun, end_evt);

  auto log        = logger.get_log();
  std::string exp = "(0, 1)(1, 4, 5)(6, 1)(7, 1, 6)(13, 1)(14, 1)(15, 1)(16, 1)";
  ASSERT_EQ(exp, log);
}