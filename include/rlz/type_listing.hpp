#pragma once

#include <algorithm>
#include <list>
#include <string>
#include <sstream>
#include <vector>

#include "generic_caller.hpp"
#include "type_utils.hpp"

#define REGISTER(Id, Type, Name) struct Id { using type = Type; static std::string name() { return Name""; } }
#define LIST(Id, ...) using Id = rlz::type_utils::type_list<__VA_ARGS__>
#define CALLER(...) using Caller = rlz::utils::Caller<__VA_ARGS__>

namespace rlz { namespace utils {

template <typename T>
struct options_getter { };

template <typename Head, typename... Tail>
struct options_getter<rlz::type_utils::type_list<Head, Tail...>> {
  void operator()(std::vector<std::string> &names)
  {
    names.push_back(Head::name());
    options_getter<rlz::type_utils::type_list<Tail...>>{}(names);
  }
};

template <>
struct options_getter<rlz::type_utils::type_list<>> {
  void operator()(std::vector<std::string> &names)
  {
  }
};

template <typename Os>
Os &operator<<(Os &os, const std::vector<std::string> &v)
{
  for (auto i : v) {
    os << i << " ";
  }
  return os;
}

template <typename OpList>
std::vector<std::string> options()
{
  std::vector<std::string> names;
  options_getter<OpList>{}(names);
  return names;
}

template <typename OpList>
std::string options_string()
{
  auto names = options<OpList>();
  std::stringstream ss;
  ss << names;
  return ss.str();
}

template <typename OpList>
bool allow(std::string option)
{
  auto names = options<OpList>();
  return std::find(names.begin(), names.end(), option) != names.end();
}

// For multi-types
template <typename Call, typename It, typename Invoke>
void call(It type_begin, It type_end, Invoke &ivk)
{
  std::list<std::string> list { type_begin, type_end };
  Call{}.call(list, ivk);
}

// For a single type
template <typename Call, typename Invoke>
void call(std::string type_name, Invoke &ivk)
{
  std::vector<std::string> list {{ type_name }};
  call<Call>(list.begin(), list.end(), ivk);
}

}}