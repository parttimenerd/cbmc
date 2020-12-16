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

struct scope
{
  size_t id;
  std::unordered_set<dstringt> assigned;

  explicit scope(size_t id) : id(id)
  {
  }

  void assign(dstringt id);
};

class loop_stack;

struct loop_iteration
{
  size_t id;

  loop_stack *stack;

  scope &before_end;

  scope &start;

  scope &end;

  loop_iteration(
    size_t id,
    loop_stack *stack,
    scope &before_end,
    scope &start,
    scope &end)
    : id(id), stack(stack), before_end(before_end), start(start), end(end)
  {
  }

  std::vector<dstringt> assigned_variables() const;

  std::vector<dstringt> outer_loop_variables() const;

  friend std::ostream &
  operator<<(std::ostream &os, const loop_iteration &iteration);
};

class loop_stack
{
  size_t max_loop_id = 0;

  std::vector<scope> scopes;

  std::vector<loop_iteration> iterations;

  std::vector<size_t> before_ends;

public:
  const std::vector<scope> &get_scopes()
  {
    return scopes;
  }

  loop_stack()
  {
    push_back_scope();
  }

  scope &current_scope()
  {
    return scopes.back();
  }

  void push_back_scope()
  {
    scopes.emplace_back(scopes.size());
  }

  void push_back_loop()
  {
    before_ends.push_back(current_scope().id);
    push_back_scope();
    //std::cout << "start loop\n";
  }

  scope &pop_loop()
  {
    auto before_end = before_ends.back();
    before_ends.pop_back();
    return scopes.at(before_end);
  }

  void push_last_loop_iteration()
  {
    push_back_scope();
    iterations.emplace_back(
      max_loop_id, this, pop_loop(), current_scope(), current_scope());
    max_loop_id++;
  }

  void pop_last_loop_iteration()
  {
    auto &last = iterations.back();
    last.end = current_scope();
    std::cout << last << "\n";
    //std::cout << "-> pop loop\n";
    iterations.pop_back();
    push_back_scope();
  }

  void assign(dstringt id)
  {
    //std::cout << id << "\n";
    current_scope().assign(id);
  }

  std::vector<dstringt> variables(size_t start_scope, size_t end_scope);
};

#endif //CBMC_LOOPSTACK_HPP
