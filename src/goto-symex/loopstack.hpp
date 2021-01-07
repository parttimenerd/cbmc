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
  /// is part of the assigned variables
  optionalt<dstringt> guard;
  std::unordered_set<dstringt> assigned;

  explicit scope(size_t id) : id(id)
  {
  }

  void assign(dstringt id);

  /// does assigning the passed variable lead to an incosistent state
  /// and is it therefore necessary to create a new scope?
  bool split_before(dstringt id) const;

  bool matches_guard(dstringt guard_var) const;
};

class loop_stack;

/// last loop iteration
struct loop_iteration
{
  size_t id;

  loop_stack *stack;

  size_t before_end_scope;

  size_t start_scope;

  size_t end_scope;

  optionalt<guard_exprt> guard;

  /// variables assigned via phis after the whole unrolled loop
  /// that have versions inside the loop and are not constant
  std::unordered_set<dstringt> used_after;

  loop_iteration(
    size_t id,
    loop_stack *stack,
    size_t before_end_scope,
    size_t start_scope,
    size_t end_scope);

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

  bool is_first_iter_guard(dstringt var);

  /// returns the end that omits the first loop iteration
  /// works if the guard is already set
  size_t adjusted_end_scope() const;

  exprt first_guard()
  {
    return guard.value().first_guard();
  }

  void add_used_after(dstringt var)
  {
    if(getenv("LOG_LOOP_MERGE") != nullptr || getenv("LOG_LOOP"))
    {
      std::cerr << guard.value().as_expr().to_string2() << " use after " << var
                << "\n";
    }
    used_after.emplace(var);
  }
};

struct aborted_recursion
{
  std::unordered_set<dstringt> parameters;
  optionalt<dstringt> return_var;
  std::string func_id;

  explicit aborted_recursion(const std::string &func_id) : func_id(func_id)
  {
  }

  bool assign_return(dstringt id);

  void assign(dstringt id)
  {
    assert(!return_var);
    parameters.emplace(id);
  }

  friend std::ostream &
  operator<<(std::ostream &os, const loop_iteration &iteration);
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

  std::vector<aborted_recursion> recursions;

  std::vector<std::vector<dstringt>> relations;

  optionalt<aborted_recursion> current_recursion;
  bool current_recursion_waits_for_return = false;

public:
  const std::vector<scope> &get_scopes()
  {
    return scopes;
  }

  const scope &get_scope(size_t id) const
  {
    return scopes.at(id);
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
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "start loop " << current_scope().id << "\n";
    }
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
      max_loop_id, this, pop_loop().id, current_scope().id, current_scope().id);
    iteration_stack.push_back(max_loop_id);
    max_loop_id++;
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "push last loop iteration \n";
    }
  }

  void pop_last_loop_iteration()
  {
    auto &last = iterations.at(iteration_stack.back());
    last.end_scope = current_scope().id;
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "end loop " << last.id << "\n";
    }
    iteration_stack.pop_back();
    push_back_scope();
  }

  void assign(dstringt id);

  void push_aborted_recursion(std::string function_id)
  {
    assert(!current_recursion);
    current_recursion = aborted_recursion(function_id);
  }

  void pop_aborted_recursion()
  {
    current_recursion_waits_for_return = true;
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
  bool should_discard_assignments_to(const dstringt &lhs);

  /// Relate the symbol to all symbols that are part of the passed expression
  void relate(std::vector<dstringt> symbols, exprt expr);
};

#endif //CBMC_LOOPSTACK_HPP
