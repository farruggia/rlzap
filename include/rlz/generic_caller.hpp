#pragma once

#include <list>
#include <stdexcept>

#include "type_utils.hpp"

namespace rlz { namespace utils {

using rlz::type_utils::type_list;

// Caller
template <typename...>
struct type_collection { };

template <typename...>
struct Caller { };

// Start collecting
template <typename... L_Types, typename... Others>
struct Caller<type_list<L_Types...>, Others...>
{
  template <typename Call>
  void call(std::list<std::string> names, Call &c)
  {
    Caller<type_collection<>, type_list<L_Types...>, Others...>{}.template call(names, c);
  }  
};

// Middle of match
template <typename... Partial, typename L_Head, typename... L_Tail, typename... Others>
struct Caller<type_collection<Partial...>, type_list<L_Head, L_Tail...>, Others...>
{
  template <typename Call>
  void call(std::list<std::string> names, Call &c)
  {
    if (names.empty()) {
      throw std::logic_error("Not enough type names in list");
    } else if (names.front() == L_Head::name()) {
      names.pop_front();
      Caller<type_collection<Partial..., typename L_Head::type>, Others...>{}.template call(names, c);
    } else {
      Caller<type_collection<Partial...>, type_list<L_Tail...>, Others...>{}.template call(names, c);
    }
  }  
};

// Failure on a list
template <typename... Partial, typename... Others>
struct Caller<type_collection<Partial...>, type_list<>, Others...>
{
  template <typename Call>
  void call(std::list<std::string> names, Call &c)
  {
    throw std::logic_error("Could not match type " + names.front());
  }  
};


// End
template <typename... Partial>
struct Caller<type_collection<Partial...>>
{
  template <typename Call>
  void call(std::list<std::string> names, Call &c)
  {
    if (names.empty()) {
      c.template call<Partial...>();
    } else {
      throw std::logic_error("Could not match all requested names");
    }
  }  
};

}}