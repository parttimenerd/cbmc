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

template <typename baset>
class indirection
{
  std::vector<baset> &base;
  size_t index;

public:
  indirection(std::vector<baset> &base, size_t index) : base(base), index(index)
  {
  }

  baset &operator*()
  {
    return base.at(index);
  }
};

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

class loopt;

/// last loop iteration
struct loop_iteration
{
  const size_t id;

  loopt *loop;

  const size_t start_scope;

  size_t end_scope;

  optionalt<guard_exprt> guard;

  /// variables assigned via phis after the whole unrolled loop
  /// that have versions inside the loop and are not constant
  std::unordered_set<dstringt> used_after;

  /// is this the last iteration of the unrolled loop?
  const bool is_second_to_last_iteration;

  loop_iteration(
    size_t id,
    loopt *loop,
    size_t start_scope,
    size_t end_scope,
    bool is_last_iteration);

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

  exprt first_guard()
  {
    return guard.value().first_guard();
  }

  void add_used_after(dstringt var);

  bool is_last_iteration() const;
};

class loop_stackt;

struct loopt
{
  const size_t id;

  const loop_stackt *stack;

  const size_t depth;

  /// omit the very first iteration
  std::vector<loop_iteration> iterations;

  std::vector<guard_exprt> guards;

  const size_t before_end_scope;

  std::vector<std::vector<dstringt>> relations;

  loopt(
    size_t id,
    loop_stackt *stack,
    const size_t depth,
    size_t before_end_scope);

  /// returns the end that omits the first loop iteration
  /// works if the guard is already set
  size_t adjusted_end_scope() const;

  loop_iteration &back()
  {
    return iterations.back();
  }

  const loop_iteration &back() const
  {
    return iterations.back();
  }

  const scope &get_scope(size_t scope_id) const;

  const loop_stackt *get_stack() const;

  friend std::ostream &operator<<(std::ostream &os, const loopt &loop);

  dstringt first_guard() const
  {
    return to_symbol_expr(guards.front().last_guard()).get_identifier();
  }
  void
  push_loop(size_t end_scope_of_last, size_t end_scope, bool is_last_iteration);
  void end_last_iteration(size_t end_scope);
  void add_guard(guard_exprt &iter_guard);

  /// Relate the symbol to all symbols that are part of the passed expression
  void relate(std::vector<dstringt> symbols, exprt expr);
  std::vector<dstringt> outer_loop_variables();
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

class loop_stackt
{
  std::vector<scope> scopes;

  std::vector<loopt> loops;

  std::vector<size_t> loop_stack;

  std::map<exprt, size_t> last_guard_to_loop{};

  std::vector<aborted_recursion> recursions;

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

  loop_stackt()
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
    auto before_id = current_scope().id;
    push_back_scope();
    loops.emplace_back(loops.size(), this, loop_stack.size(), before_id);
    loop_stack.push_back(loops.back().id);
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "start loop " << current_loop().id << "\n";
    }
  }

  loopt &current_loop()
  {
    return loops.at(loop_stack.back());
  }

  void push_loop_iteration(bool is_last_iteration)
  {
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "push iteration " << (current_loop().iterations.size() + 1)
                << " of loop " << current_loop().id;
      if(is_last_iteration)
      {
        std::cerr << " last iteration";
      }
      std::cerr << "\n";
    }
    push_back_scope();
    current_loop().push_loop(
      current_scope().id - 1, current_scope().id, is_last_iteration);
  }

  void pop_last_loop_iteration()
  {
    auto &last = current_loop();
    last.end_last_iteration(current_scope().id);
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "end loop " << current_loop().id << "\n";
    }
    loop_stack.pop_back();
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

  std::vector<dstringt> variables(size_t start_scope, size_t end_scope) const;

  /// Set the guard of the last loop iteration that is currently on top
  /// if it does not yet have a loop iteration
  ///
  /// \param guard guard (should consist of a conjuction of guards from the outer most to the inner most if expression)
  void set_iter_guard(guard_exprt &guard);

  /// Returns the last loop iteration for a given guard expr (must match
  /// the last guard of a loop iteration)
  /// \return nullptr if no loop iteration found
  loopt *get_iter_for_last_guard(exprt guard_expr);

  void emit(std::ostream &os);

  ~loop_stackt()
  {
    emit(std::cout);
  }
  bool should_discard_assignments_to(const dstringt &lhs);
};

#endif //CBMC_LOOPSTACK_HPP
