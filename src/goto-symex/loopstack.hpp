/*******************************************************************\

Module: Stack of variables accessed and altered in loops

Author: Johannes Bechberger, johannes@bechberger.me

\*******************************************************************/

/// \file
/// Stack of variables accessed and altered in loops

#ifndef CBMC_LOOPSTACK_HPP
#define CBMC_LOOPSTACK_HPP

#include "analyses/guard_expr.h"
#include <expr.h>
#include <iostream>
#include <map>
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

/// last loop iteration
struct loop_iteration
{
  size_t id;

  loop_stack *stack;

  scope &before_end;

  scope &start;

  scope &end;

  optionalt<guard_exprt> guard;

  /// variables assigned via phis after the whole unrolled loop
  /// that have versions inside the loop and are not constant
  std::unordered_set<dstringt> used_after;

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

  /// Set the guard of the last loop iteration that is currently on top
  /// if it does not yet have a loop iteration
  ///
  /// \param new_guard guard (should consist of a conjuction of guards from the outer most to the inner most if expression)
  /// \return is guard updated?
  bool set_iter_guard(guard_exprt &new_guard);

  bool has_iter_guard()
  {
    return guard.has_value();
  }

  exprt first_guard()
  {
    return guard.value().first_guard();
  }

  void add_used_after(dstringt var)
  {
    std::cout << "   use after " << var << "\n";
    used_after.emplace(var);
  }
};

class loop_stack
{
  size_t max_loop_id = 0;

  std::vector<scope> scopes;

  std::vector<loop_iteration> iterations;

  std::vector<size_t> iteration_stack;

  std::vector<size_t> before_ends;

  std::map<exprt, size_t> first_guard_to_iter{};
  std::map<exprt, size_t> last_guard_to_iter{};

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
    iteration_stack.push_back(max_loop_id);
    max_loop_id++;
  }

  void pop_last_loop_iteration()
  {
    auto &last = iterations.at(iteration_stack.back());
    last.end = current_scope();
    std::cout << last << "\n";
    //std::cout << "-> pop loop\n";
    iteration_stack.pop_back();
    push_back_scope();
  }

  void assign(dstringt id)
  {
    if(id.empty())
    {
      throw "empty var";
    }
    current_scope().assign(id);
  }

  std::vector<dstringt> variables(size_t start_scope, size_t end_scope);

  /// Set the guard of the last loop iteration that is currently on top
  /// if it does not yet have a loop iteration
  ///
  /// \param guard guard (should consist of a conjuction of guards from the outer most to the inner most if expression)
  void set_iter_guard(guard_exprt &guard);

  /// Returns the last loop iteration for a given guard expr (must match
  /// the first guard of a loop iteration)
  /// \return nullptr if no loop iteration found
  loop_iteration *get_iter_for_first_guard(exprt guard_expr);

  /// Returns the last loop iteration for a given guard expr (must match
  /// the last guard of a loop iteration)
  /// \return nullptr if no loop iteration found
  loop_iteration *get_iter_for_last_guard(exprt guard_expr);

  void emit(std::ostream &os);

  ~loop_stack()
  {
    emit(std::cout);
  }
};

#endif //CBMC_LOOPSTACK_HPP
