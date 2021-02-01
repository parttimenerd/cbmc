/*******************************************************************\

Module: Stack of variables accessed and altered in loops

Author: Johannes Bechberger, johannes@bechberger.me

\*******************************************************************/

/// \file
/// Stack of variables accessed and altered in loops

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

class variablest
{
  std::unordered_map<std::string, std::set<size_t>> vars;

public:
  variablest()
  {
  }

  variablest(std::unordered_set<dstringt> vars)
  {
    for(const auto &var : vars)
    {
      insert(var);
    }
  }

  void insert(dstringt var)
  {
    insert(std::string(var.c_str()));
  }

  std::tuple<std::string, size_t> split(const std::string &name) const
  {
    auto middle = name.find("#");
    std::istringstream iss(name.substr(middle + 1));
    size_t num;
    iss >> num;
    return {name.substr(0, middle), num};
  }

  void insert(const std::string &var)
  {
    auto split_res = split(var);
    vars[std::get<0>(split_res)].insert(std::get<1>(split_res));
  }

  bool contains(const std::string &var)
  {
    auto split_res = split(var);
    return vars[std::get<0>(split_res)].find(std::get<1>(split_res)) !=
           vars[std::get<0>(split_res)].end();
  }

  bool contains_guard() const
  {
    auto it = vars.find("goto_symex::\\guard");
    return it != vars.end() && !it->second.empty();
  }

  std::vector<std::string> get_first() const
  {
    std::vector<std::string> ret;
    for(const auto &item : vars)
    {
      if(!item.second.empty())
      {
        ret.push_back(item.first + "#" + std::to_string(*item.second.begin()));
      }
    }
    return ret;
  }

  std::vector<std::string> get_last() const
  {
    std::vector<std::string> ret;
    for(const auto &item : vars)
    {
      if(!item.second.empty())
      {
        ret.push_back(item.first + "#" + std::to_string(*item.second.rbegin()));
      }
    }
    return ret;
  }

  /**
   * Create a new variable set that contains only the variables that the other variable set contains,
   * albeit with different numbers
   */
  variablest restrict_to(const variablest &other) const;

  optionalt<std::string> get_last_guard() const;

  friend std::ostream &
  operator<<(std::ostream &os, const variablest &variablest);
};

class symex_target_equationt;

struct scope
{
  size_t id;
  /// is part of the assigned variables
  optionalt<dstringt> guard;
  std::unordered_set<dstringt> assigned;
  variablest variables;
  std::unordered_set<dstringt> read_variables;

  explicit scope(size_t id) : id(id)
  {
  }

  void assign(dstringt id);

  void read(dstringt id);

  /// does assigning the passed variable lead to an inconsistent state
  /// and is it therefore necessary to create a new scope?
  bool split_before(dstringt id) const;

  bool matches_guard(dstringt guard_var) const;
};

struct loopt;

/// last loop iteration
struct loop_iteration
{
  const size_t id;

  loopt *loop;

  const size_t start_scope;

  size_t end_scope;

  const bool is_second_to_last_iteration;

  /// is this the last iteration of the unrolled loop?
  const bool is_last_iteration;

  loop_iteration(
    size_t id,
    loopt *loop,
    size_t start_scope,
    size_t end_scope,
    const bool is_second_to_last_iteration,
    const bool is_last_iteration);

  std::vector<dstringt> assigned_variables() const;

  variablest assigned_variable_set() const;

  bool contains_variables() const;

  optionalt<dstringt> guard() const;
};

class loop_stackt;

enum class parent_typet
{
  NONE,
  LOOP,
  RECURSION
};

struct parent_idt
{
  parent_typet type;
  size_t id;

  friend std::ostream &operator<<(std::ostream &os, const parent_idt &idt);
};

/// [(guard_var, value it is assumed to have)]
using guard_variablest = std::vector<std::tuple<dstringt, bool>>;

/// returns [(guard_var, value it is assumed to have)]
guard_variablest
get_guard_variables(const guard_exprt &guard, size_t omit_last = 0);

std::ostream &
operator<<(std::ostream &os, const guard_variablest &guard_variables);

struct loopt
{
  const size_t id;

  const dstringt func_id;

  const size_t nr;

  const parent_idt parent;

  const loop_stackt *stack;

  const size_t depth;
  /// guard in the context (remove later all guards that are part of the guards variable)
  const guardt context_guard;

  /// omits the very first iteration
  std::vector<loop_iteration> iterations;

  std::vector<guard_exprt> guards;

  const size_t before_end_scope;

  std::vector<std::vector<dstringt>> relations;

  /// variables assigned via phis after the whole unrolled loop
  /// that have versions inside the loop and are not constant
  std::unordered_set<dstringt> used_after;
  variablest used_after_variables;

  loopt(
    size_t id,
    const dstringt func_id,
    size_t nr,
    parent_idt parent,
    loop_stackt *stack,
    const size_t depth,
    guardt context_guard,
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

  bool has_guards() const
  {
    return !guards.empty();
  }

  optionalt<dstringt> first_guard() const
  {
    if(has_guards())
    {
      return {to_symbol_expr(guards.front().last_guard()).get_identifier()};
    }
    return {};
  }
  void push_iteration(
    size_t end_scope_of_last,
    size_t end_scope,
    bool is_second_to_last_iteration,
    bool is_last_iteration);
  void end_loop(size_t end_scope);
  void add_guard(guard_exprt &iter_guard);

  /// Relate the symbol to all symbols that are part of the passed expression
  void relate(std::vector<dstringt> symbols, exprt expr);
  std::vector<dstringt> outer_loop_variables() const;

  bool in_second_to_last_iteration()
  {
    return !iterations.empty() && back().is_second_to_last_iteration;
  }
  bool in_last_iteration()
  {
    return !iterations.empty() && back().is_last_iteration;
  }

  void add_used_after(dstringt var);

  std::vector<std::string> last_iter_input() const;

  std::vector<std::string> last_iter_output() const;

  std::vector<std::string> result_variables() const;
};

struct recursiont;

/// Aborted last recursion
/// it is related an abstract recursion iff the abstraction is used
/// It knows the global variables that it reads and writes
class aborted_recursiont
{
public:
  const loop_stackt *stack;
  const size_t id;
  const dstringt func_id;
  const parent_idt parent;
  const optionalt<recursiont *> recursion;
  /// guard expression that holds true when the function call is applied
  /// important for later analyses
  /// without it `bool fib(bool x){return x ? fib(x) : x;}}` would over approximate the variability that goes
  /// into `fib(x)` (with REC=0)
  const guardt guard;

private:
  /// (non constant?) parameter variables
  std::unordered_set<dstringt> parameters;
  /// read globals
  std::unordered_set<dstringt> read_globals;
  /// return value if used and if present
  optionalt<dstringt> return_var;
  /// written globals
  std::unordered_set<dstringt> written_globals;

public:
  explicit aborted_recursiont(
    const loop_stackt *stack,
    size_t id,
    dstringt funcId,
    parent_idt parent,
    guardt guard,
    optionalt<recursiont *> recursion = {})
    : stack(stack),
      id(id),
      func_id(std::move(funcId)),
      parent(parent),
      recursion(std::move(recursion)),
      guard(std::move(guard))
  {
  }

  bool assign_return(dstringt var);

  void assign_parameter(dstringt var)
  {
    assert(!return_var);
    parameters.emplace(var);
  }

  /// Resolve all read globals and their current instantiation
  void assign_read_globals(const std::function<dstringt(dstringt)> &resolve);

  /// get variable base names of possibly modified global variables
  /// (non local variables)
  /// assign them new names
  std::unordered_set<dstringt> get_assigned_global_variable_base_names() const;

  /// creates an unknown variable for each written global
  void assign_written_globals(
    const std::function<dstringt(dstringt)> &&create_unknown);

  /// returns [(guard_var, value it is assumed to have)]
  std::vector<std::tuple<dstringt, bool>> get_guard_variables() const;

  friend std::ostream &
  operator<<(std::ostream &os, const aborted_recursiont &recursion);

protected:
  /// parameters + read globals
  std::unordered_set<dstringt> combined_read_vars() const;

  /// return value + written globals
  std::unordered_set<dstringt> combined_written_vars() const;
};

class loop_stackt
{
  std::vector<scope> scopes;

  std::vector<loopt> loops;

  std::vector<size_t> loop_stack;

  std::map<symbol_exprt, size_t> guard_symbol_to_loop{};

  std::vector<aborted_recursiont> aborted_recursions;

  optionalt<aborted_recursiont *> current_aborted_recursion_;
  bool current_recursion_waits_for_return = false;

  std::vector<parent_idt> parent_ids;

  optionalt<ls_infot> info;
  std::unique_ptr<ls_recursion_node_dbt> rec_nodes;

  bool is_initialized = false;

public:
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

  const ls_infot get_info() const
  {
    assert(is_initialized);
    return info.value();
  }

  ls_recursion_node_dbt &abstract_recursion()
  {
    return *rec_nodes;
  }

  bool make_second_to_last_iteration_abstract() const
  {
    return true;
  }

  const std::vector<scope> &get_scopes()
  {
    return scopes;
  }

  const std::vector<loopt> &get_loops()
  {
    return loops;
  }

  const scope &get_scope(size_t id) const
  {
    return scopes.at(id);
  }

  loop_stackt()
  {
    parent_ids.push_back(parent_idt{parent_typet::NONE, 0});
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

  void
  push_back_loop(dstringt func_id, size_t loop_nr, guard_exprt context_guard)
  {
    assert(is_initialized);
    auto before_id = current_scope().id;
    push_back_scope();
    loops.emplace_back(
      loops.size(),
      func_id,
      loop_nr,
      parent_ids.back(),
      this,
      loop_stack.size(),
      context_guard,
      before_id);
    loop_stack.push_back(loops.back().id);
    parent_ids.emplace_back(parent_idt{parent_typet::LOOP, loops.back().id});
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "start loop " << current_loop().id << "\n";
    }
  }

  loopt &current_loop()
  {
    return loops.at(loop_stack.back());
  }

  bool is_in_loop()
  {
    return !loop_stack.empty();
  }

  void
  push_loop_iteration(bool is_second_to_last_iteration, bool is_last_iteration)
  {
    assert(is_initialized);
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
    current_loop().push_iteration(
      current_scope().id - 1,
      current_scope().id,
      is_second_to_last_iteration,
      is_last_iteration);
  }

  void end_current_loop()
  {
    auto &last = current_loop();
    last.end_loop(current_scope().id);
    if(getenv("LOG_LOOP") != nullptr)
    {
      std::cerr << "end loop " << current_loop().id << "\n";
    }
    loop_stack.pop_back();
    parent_ids.pop_back();
    push_back_scope();
  }

  void assign(dstringt id);

  void read(dstringt id);

  /// Add an aborted recursion
  /// uses the state to obtain the guards
  /// resolve: var base name to name
  void push_aborted_recursion(
    dstringt function_id,
    const goto_symex_statet &state,
    std::function<dstringt(dstringt)> &&resolve);

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
  loopt *get_loop_for_guard_symbol(exprt guard_expr);

  void emit(std::ostream &os);

  ~loop_stackt()
  {
    emit(std::cout);
  }

  /// Should an assignment to the past variable lead to a self assignment.
  ///
  /// An assignment should be removed if it is related to the return value of an aborted recursion,
  /// or if it is related to the second to last iteration (and its configured to use this iteration as an abstract one)
  bool should_discard_assignments_to(const dstringt &lhs);

  bool in_aborted_recursion() const
  {
    return current_aborted_recursion_.has_value();
  }

  /// returns null if not currently processing aborted recursion
  aborted_recursiont *current_aborted_recursion()
  {
    return current_aborted_recursion_ ? current_aborted_recursion_.value()
                                      : nullptr;
  }
};

#endif //CBMC_LOOPSTACK_HPP
