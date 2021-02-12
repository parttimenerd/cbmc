/*******************************************************************\

Module: Abstraction of aborted recursions, corresponds with the recursion_graph module in dsharpy

Author: Johannes Bechberger, johannes.bechberger@kit.edu

\*******************************************************************/

#ifndef CBMC_LS_REC_GRAPH_H
#define CBMC_LS_REC_GRAPH_H

#include "analyses/guard.h"
#include "ls_info.h"
#include <cstring>
#include <dstring.h>
#include <ostream>
#include <utility>

using name_mappingt = std::unordered_map<dstringt, dstringt>;
using resolvet = std::function<dstringt(dstringt)>;
using assign_unknownt = std::function<void(dstringt)>;

bool is_guard(dstringt var);

/// should a loop be marked to be fully approximated (without counting the real variability of the inputs)?
/// helpful for loops that are known to be unimportant
bool should_fully_over_approximate(dstringt function_id);

template <typename Container>
name_mappingt create_mapping(
  const Container &variables,
  const std::function<dstringt(dstringt)> &resolve,
  bool skip_guards = false)
{
  name_mappingt ret;
  for(auto var : variables)
  {
    if(skip_guards && is_guard(var))
    {
      continue;
    }
    ret.emplace(var, resolve(var));
  }
  return ret;
}

dstringt basename(const dstringt &name);
dstringt l0_name(const dstringt &name);

std::tuple<dstringt, size_t> split_var(const dstringt &name);

std::vector<dstringt> get_base_names(const name_mappingt &mapping);

std::ostream &operator<<(std::ostream &os, const name_mappingt &mapping);

std::string to_string(const name_mappingt &mapping);

bool is_recursion_graph_enabled();

/// Returns the times a called function should inlined in an abstract call
unsigned int recursion_graph_inlining();

class ls_recursion_baset
{
public:
  const ls_func_info &info;
  const dstringt func_name;

protected:
  const name_mappingt input;
  const name_mappingt output;

  ls_recursion_baset(
    const ls_func_info &info,
    name_mappingt input,
    name_mappingt output)
    : info(info),
      func_name(info.function_id),
      input(std::move(input)),
      output(std::move(output))
  {
  }
};

/// an aborted recursion that references a function
/// create via the create method and finish it via the assign_written method
class ls_recursion_childt : public ls_recursion_baset
{
  const size_t id;
  const guardt guard;

  ls_recursion_childt(
    size_t id,
    const ls_func_info &info,
    name_mappingt input,
    name_mappingt output,
    const guardt &guard)
    : ls_recursion_baset(info, std::move(input), std::move(output)),
      id(id),
      guard(guard)
  {
  }

public:
  friend std::ostream &
  operator<<(std::ostream &os, const ls_recursion_childt &child);

  /// creates a new rec child
  ///
  /// \param resolve: used to resolve parameters and read globals variables
  static ls_recursion_childt create(
    size_t id,
    const ls_func_info &info,
    const guardt &guard,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown);
};

/// an abstract recursive function
/// create via the create method and finish it via the assign_written method
class ls_recursion_nodet : public ls_recursion_baset
{
public:
  ls_recursion_nodet(
    const ls_func_info &info,
    name_mappingt input,
    name_mappingt output)
    : ls_recursion_baset(info, std::move(input), std::move(output))
  {
  }

public:
  friend std::ostream &
  operator<<(std::ostream &os, const ls_recursion_nodet &nodet);
};

struct requested_functiont
{
  const symbol_exprt identifier;

  dstringt name() const
  {
    return identifier.get_identifier();
  }

  bool operator==(const requested_functiont &rhs) const
  {
    return identifier == rhs.identifier;
  }

  bool operator!=(const requested_functiont &rhs) const
  {
    return !(rhs == *this);
  }
};

// NOLINTNEXTLINE(readability/namespace)
namespace std
{
template <>
// NOLINTNEXTLINE(readability/identifiers)
struct hash<requested_functiont>
{
  std::size_t operator()(const requested_functiont &func) const
  {
    return std::hash<dstringt>{}(func.name());
  }
};
} // namespace std

class bring_back_exceptiont : public std::exception
{
public:
  bring_back_exceptiont() : std::exception()
  {
  }
};

/// Stores recursion nodes
class ls_recursion_node_dbt
{
  const ls_infot &info;
  std::unordered_map<dstringt, ls_recursion_nodet> nodes;

  std::vector<ls_recursion_childt> rec_children;

  std::unordered_set<requested_functiont> requested_funcs;

  bool in_abstract_processing = false;

public:
  explicit ls_recursion_node_dbt(const ls_infot &info) : info(info)
  {
  }

  void create_rec_child(
    const requested_functiont &func,
    const guardt &guard,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown)
  {
    auto child = ls_recursion_childt::create(
      rec_children.size(),
      info.get_func_info(func.name()),
      guard,
      resolve,
      assign_unknown);
    rec_children.push_back(std::move(child));
    request(func);
  }

  void request(const requested_functiont &func)
  {
    if(requested_funcs.find(func) == requested_funcs.end() && enabled())
    {
      requested_funcs.emplace(func);
    }
  }

  const std::unordered_set<requested_functiont> &requested() const
  {
    return requested_funcs;
  }

  /// setup the current frame before calling it
  /// it sets all needed variables, runs the passed method and records the values of variables
  void process_requested(
    const requested_functiont &func,
    const std::function<void()> &state_processor,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown);

  friend std::ostream &
  operator<<(std::ostream &os, const ls_recursion_node_dbt &dbt);

  const std::unordered_map<dstringt, ls_recursion_nodet> &get_nodes() const
  {
    return nodes;
  }

  /// enabled === output abstract recursions
  bool enabled() const
  {
    return is_recursion_graph_enabled();
  }

  unsigned int inlining_depth() const
  {
    return recursion_graph_inlining();
  }

  bool is_in_abstract_processing() const
  {
    return in_abstract_processing;
  }
};

#endif //CBMC_LS_REC_GRAPH_H
