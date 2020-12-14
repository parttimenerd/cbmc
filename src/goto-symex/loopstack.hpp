/*******************************************************************\

Module: Stack of variables accessed and altered in loops

Author: Johannes Bechberger, johannes@bechberger.me

\*******************************************************************/

/// \file
/// Stack of variables accessed and altered in loops

#ifndef CBMC_LOOPSTACK_HPP
#define CBMC_LOOPSTACK_HPP

#include <expr.h>
#include <iostream>
#include <unordered_set>
#include <vector>

class symex_target_equationt;

class ls_element
{
  size_t eid;

  std::unordered_set<dstringt> accessed;
  std::unordered_set<dstringt> assigned;

public:
  ls_element(size_t id);

  void emit();

  void assign(dstringt id);

  void access(dstringt id);

  void access(const exprt *expr);

  void merge(ls_element &inner);
};

class loop_stack
{
  size_t max_id = 0;

  std::vector<ls_element> elements;

public:
  void push()
  {
    elements.emplace_back(max_id);
    max_id++;
  }

  void pop()
  {
    auto &last = elements.back();
    if(elements.size() > 1)
    {
      elements.at(elements.size() - 2).merge(last);
    }
    last.emit();
    elements.pop_back();
  }

  ls_element &top()
  {
    return elements.front();
  }

  void assign(dstringt id)
  {
    if(has_loop())
    {
      std::cout << "assign " << id << "\n";
      top().assign(id);
    }
  }

  void access(const dstringt id)
  {
    if(has_loop())
    {
      top().access(id);
    }
  }

  void access(const exprt *expr)
  {
    if(has_loop())
    {
      top().access(expr);
    }
  }

  bool has_loop()
  {
    return !elements.empty();
  }
};

#endif //CBMC_LOOPSTACK_HPP
