//
// Created by bechberger-local on 11.12.20.
//

#include "loopstack.hpp"
#include "symex_target_equation.h"

ls_element::ls_element(size_t id) : eid(id)
{
}

void ls_element::emit()
{
  std::cout << "c loop assigned " << eid;
  for(const auto &a : assigned)
  {
    std::cout << " " << a;
  }
  std::cout << "\n";
  std::cout << "c loop accessed " << eid;
  for(const auto &a : accessed)
  {
    std::cout << " " << a;
  }
  std::cout << "\n";
}

void ls_element::assign(dstringt id)
{
  assigned.emplace(id);
}

void ls_element::access(dstringt id)
{
  std::cout << "# -> " << id << "\n";
  accessed.emplace(id);
}

void ls_element::merge(ls_element &inner)
{
  accessed.insert(inner.accessed.begin(), inner.accessed.end());
  assigned.insert(inner.assigned.begin(), inner.assigned.end());
}

template <class T>
void collect_identifiers(
  const dstringt name,
  const T &map,
  std::unordered_set<dstringt> &collected)
{
  for(const auto &item : map)
  {
    const irept &expr = item.second;
    //std::cout << "access " << name << " "  << item.first << " " << expr.id() << "\n";
    if(
      expr.is_not_nil() && item.first == ID_identifier &&
      (name == ID_symbol || name == ID_nondet_symbol || name == ID_next_symbol))
    {
      collected.emplace(expr.id());
    }
    else if(item.first == ID_identifier)
    {
      //std::cout << "discarded access to " << expr.id() << "\n";
    }
    collect_identifiers(item.first, item.second.get_named_sub(), collected);
  }
}

void ls_element::access(const exprt *expr)
{
  std::unordered_set<dstringt> collected;
  collect_identifiers(expr->id(), expr->get_named_sub(), collected);
  for(const auto item : collected)
  {
    access(item);
  }
}
