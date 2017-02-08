#pragma once

#include <string>
#include <boost/core/demangle.hpp>

namespace rlz {
namespace util {
template <typename Type>
std::string type_name(const Type &v)
{
  return boost::core::demangle(typeid(v).name());
}
}
}
