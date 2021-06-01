/*******************************************************************\

Module: Stack of variables accessed and altered in loops

Author: Johannes Bechberger, johannes.bechberger@kit.edu

\*******************************************************************/

/// \file
/// Stack of variables accessed and altered in loops and helper classes

#ifndef CBMC_LOOPSTACK_HPP
#define CBMC_LOOPSTACK_HPP

#include "analyses/guard_expr.h"
#include "goto_symex_state.h"
#include "ls_info.h"
#include "ls_rec_graph.h"
#include <expr.h>
#include <iostream>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

/// a set of variables (and there different L3 versions related to their L2 versions)
class variablest
{
  std::unordered_map<dstringt, std::set<size_t>> var_map;
  std::unordered_set<dstringt> variables;

public:
  template <typename Container>
  explicit variablest(Container vars)
  {
    for(const auto &var : vars)
    {
      insert(var);
    }
  }

  void insert(dstringt var);

  bool contains(const dstringt &var) const
  {
    return var_map.find(var) != var_map.end() ||
           variables.find(var) != variables.end();
  }

  std::unordered_map<dstringt, size_t> get_first() const;

  std::vector<dstringt>
  get_var_bases_not_in(const std::unordered_map<dstringt, size_t> &&vars) const;

  std::vector<dstringt> get_var_bases() const;

  friend std::ostream &
  operator<<(std::ostream &os, const variablest &variablest);
};

class symex_target_equationt;

/// A scope (related to a specific loop iteration or recursion)
/// with an optional guard and assigned and accessed variables
class scopet
{
public:
  const size_t id;

private:
  /// is part of the assigned variables
  optionalt<dstringt> guard;
  std::unordered_set<dstringt> assigned;
  std::unordered_set<dstringt> accessed;

public:
  explicit scopet(size_t id) : id(id)
  {
  }

  void assign(dstringt id);

  const std::unordered_set<dstringt> &get_assigned() const
  {
    return assigned;
  }
  const std::unordered_set<dstringt> &get_accessed() const
  {
    return accessed;
  }

  void access(dstringt id);

  /// does assigning the passed variable lead to an inconsistent state
  /// and is it therefore necessary to create a new scope?
  bool split_before(dstringt id) const;

  bool matches_guard(dstringt guard_var) const;
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec);

class loop_iter_variablest
{
  variablest accessed;
  variablest written;
  /// a version of this variable is accessed but not written in this iteration
  std::vector<dstringt> read_bases;

public:
  loop_iter_variablest(const variablest &accessed, const variablest &written)
    : accessed(accessed),
      written(written),
      read_bases(accessed.get_var_bases_not_in(written.get_first()))
  {
  }

  const variablest &get_accessed() const
  {
    return accessed;
  }

  const variablest &get_written() const
  {
    return written;
  }

  friend std::ostream &
  operator<<(std::ostream &os, const loop_iter_variablest &loop_iter_variables)
  {
    return os << "LIV{accessed=" << loop_iter_variables.accessed
              << " written=" << loop_iter_variables.written << "}";
  }
};

class loopt;

/// a loop iteration
class loop_iterationt
{
public:
  const size_t id;

  const loopt *loop;

  const size_t start_scope;

private:
  size_t end_scope;

  mutable optionalt<loop_iter_variablest> variables; // used for caching

  optionalt<dstringt> guard;

public:
  loop_iterationt(size_t id, loopt *loop, size_t start_scope, size_t end_scope);

  bool contains_variables() const;

  /// a version of this variable is accessed but not written in this iteration
  const loop_iter_variablest &get_variables() const;

  size_t get_end_scope() const
  {
    return end_scope;
  }

  void set_end_scope(size_t end_scope_)
  {
    variables = {};
    this->end_scope = end_scope_;
  }

  const optionalt<dstringt> &get_guard() const
  {
    return guard;
  }

  void set_guard(optionalt<dstringt> guard)
  {
    this->guard = std::move(guard);
  }
};

/// the last loop iteration
class last_loop_itert
{
  loop_iterationt *iteration;

  optionalt<dstringt> guard;
  /// input at the beginning of the last iteration
  name_mappingt input;
  /// assigned unknowns for all input variables
  name_mappingt inner_input;
  name_mappingt misc_input;
  name_mappingt inner_output;
  name_mappingt output;

public:
  last_loop_itert(
    loop_iterationt *iteration,
    name_mappingt input,
    name_mappingt inner_input,
    name_mappingt misc_input)
    : iteration(iteration),
      input(std::move(input)),
      inner_input(std::move(inner_input)),
      misc_input(std::move(misc_input))
  {
  }

  void set_guard(optionalt<dstringt> guard)
  {
    this->guard = std::move(guard);
  }

  void set_output(name_mappingt inner_output, name_mappingt output)
  {
    this->inner_output = std::move(inner_output);
    this->output = std::move(output);
  }

  const name_mappingt &get_input() const
  {
    return input;
  }
  const name_mappingt &get_misc_input() const
  {
    return misc_input;
  }
  const name_mappingt &get_inner_input() const
  {
    return inner_input;
  }
  const name_mappingt &get_inner_output() const
  {
    return inner_output;
  }
  const name_mappingt &get_output() const
  {
    return output;
  }
};

class loop_stackt;

/// [(guard_var, value it is assumed to have)]
using guard_variablest = std::vector<std::tuple<dstringt, bool>>;

/// returns [(guard_var, value it is assumed to have)]
guard_variablest
get_guard_variables(const guard_exprt &guard, size_t omit_last = 0);

std::ostream &
operator<<(std::ostream &os, const guard_variablest &guard_variables);

/// a loop with a set of loop iterations
class loopt
{
public:
  const size_t id;

  const dstringt func_id;

  const size_t nr;

  const optionalt<size_t> parent_loop_id;

  const loop_stackt *stack;

  const size_t depth;
  /// guard in the context (remove later all guards that are part of the guards variable)
  const guardt context_guard;

  const bool should_fully_over_approximate;

private:
  /// omits the very first iteration
  std::vector<std::unique_ptr<loop_iterationt>> iterations;

  std::vector<guard_exprt> guards;

public:
  const size_t before_end_scope;

private:
  optionalt<last_loop_itert> last_loop_iter;

public:
  loopt(
    size_t id,
    const dstringt func_id,
    size_t nr,
    optionalt<size_t> parent_loop_id,
    loop_stackt *stack,
    const size_t depth,
    guardt context_guard,
    size_t before_end_scope,
    bool should_fully_over_approximate)
    : id(id),
      func_id(func_id),
      nr(nr),
      parent_loop_id(std::move(parent_loop_id)),
      stack(stack),
      depth(depth),
      context_guard(context_guard),
      should_fully_over_approximate(should_fully_over_approximate),
      before_end_scope(before_end_scope)
  {
  }

  /// returns the end that omits the first loop iteration
  /// works if the guard is already set
  size_t adjusted_end_scope() const;

  loop_iterationt &back()
  {
    return *iterations.back();
  }

  const loop_iterationt &back() const
  {
    return *iterations.back();
  }

  const scopet &get_scope(size_t scope_id) const;

  const loop_stackt *get_stack() const;

  friend std::ostream &operator<<(std::ostream &os, const loopt &loop);

  bool has_guards() const
  {
    return !guards.empty();
  }

  optionalt<dstringt> first_guard() const;

  void push_iteration(size_t end_scope_of_last, size_t end_scope);
  void end_loop(size_t end_scope);
  void add_guard(guard_exprt &iter_guard);

  bool in_last_iteration() const
  {
    return last_loop_iter.has_value();
  }

  void process_assigned_guard_var(dstringt var);

  const loop_iterationt &first_iteration() const
  {
    PRECONDITION(!iterations.empty());
    return *iterations.front();
  }

  /// get the base names of input
  /// a loop input is a variable that
  ///   - is read in the first loop iteration
  ///       - a version of this variable is accessed but not written in this iteration
  ///   - and written in the same iteration
  ///   - to be safe: also variables that are only written are included for now   // dtodo: fix later
  /// a misc input is an input for which the last condition does not hold
  /// \return (input bases, misc input bases)
  std::tuple<std::vector<dstringt>, std::vector<dstringt>>
  get_loop_iter_input() const;

  std::vector<dstringt> get_loop_iter_output() const
  {
    return first_iteration().get_variables().get_written().get_var_bases();
  }

  /// push loop iteration before
  /// assumes that at least one other loop iteration has been pushed before
  ///
  /// creates a last_loop_itert and assigns unknown values to all loop iter input variables (that are written in
  /// a loop iteration)
  void begin_last_loop_iteration(
    const resolvet &resolve,
    const assign_unknownt &assign_unknown);

  void end_loop(const resolvet &resolve, const assign_unknownt &assign_unknown);
};

/// nested loops that form a stack
class loop_stackt
{
  std::vector<scopet> scopes;

  std::vector<std::unique_ptr<loopt>> loops;

  std::vector<size_t> loop_stack;

  optionalt<ls_infot> info;
  std::unique_ptr<ls_recursion_node_dbt> rec_nodes;

  bool is_initialized = false;

public:

  loop_stackt()
  {
    push_back_scope();
  }

  void init(const goto_functionst &functions)
  {
    if(!is_initialized)
    {
      info = ls_infot::create(functions);
      rec_nodes = std::unique_ptr<ls_recursion_node_dbt>{
        new ls_recursion_node_dbt(info.value())};
      is_initialized = true;
    }
  }

  const ls_infot &get_info() const
  {
    PRECONDITION(is_initialized);
    return info.value();
  }

  ls_recursion_node_dbt &abstract_recursion()
  {
    return *rec_nodes;
  }

  const ls_recursion_node_dbt &abstract_recursion() const
  {
    return *rec_nodes;
  }

  const std::vector<scopet> &get_scopes() const
  {
    return scopes;
  }

  const std::vector<std::unique_ptr<loopt>> &get_loops() const
  {
    return loops;
  }

  const scopet &get_scope(size_t id) const
  {
    return scopes.at(id);
  }

  scopet &current_scope()
  {
    return scopes.back();
  }

  void push_back_scope()
  {
    scopes.emplace_back(scopes.size());
  }

  void push_back_loop(
    dstringt func_id,
    dstringt calling_location,
    size_t loop_nr,
    const guard_exprt &context_guard);

  loopt &current_loop()
  {
    return *loops.at(loop_stack.back());
  }

  bool is_in_loop() const
  {
    return !loop_stack.empty();
  }

  loopt &push_loop_iteration();

  void end_current_loop(
    const resolvet &resolve,
    const assign_unknownt &assign_unknown);

  /// record assignment of variable
  void assign(dstringt id);

  /// record access of variable
  void access(dstringt id);

  std::vector<dstringt> variables(
    size_t start_scope,
    size_t end_scope,
    const std::function<std::unordered_set<dstringt>(const scopet &)>
      &accessor = [](const scopet &sc) { return sc.get_assigned(); }) const;

  /// Set the guard of the last loop iteration that is currently on top
  /// if it does not yet have a loop iteration
  ///
  /// \param guard guard (should consist of a conjuction of guards from the outer most to the inner most if expression)
  void set_iter_guard(guard_exprt &guard);

  void emit(std::ostream &os) const;

  /// destructor that emits the loops to standard out on destruction,
  /// this ensures that the loops are eventually emitted after all
  /// loops are processed
  ~loop_stackt()
  {
    emit(std::cout);
  }
};

#endif //CBMC_LOOPSTACK_HPP
