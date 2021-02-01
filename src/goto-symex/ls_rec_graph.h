/**
 * Corresponds with the recursion_graph module in dsharpy
 */

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
using set_guardt = std::function<void(guardt)>;

name_mappingt create_mapping(
  const std::unordered_set<dstringt> &variables,
  const resolvet &resolve);

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
  name_mappingt output;

  ls_recursion_baset(const ls_func_info &info, name_mappingt input)
    : info(info), func_name(info.function_id), input(std::move(input))
  {
  }

public:
  /// get variable base names of possibly modified global variables
  /// (non local variables) and the return value
  std::unordered_set<dstringt> get_written() const;
};

/// an aborted recursion that references a function
/// create via the create method and finish it via the assign_written method
class ls_recursion_childt : public ls_recursion_baset
{
  const size_t id;
  const guardt guard;

  /// parent abstract method if this occurred inside the evaluation of one
  const optionalt<dstringt> parent;

  ls_recursion_childt(
    size_t id,
    const ls_func_info &info,
    name_mappingt input,
    const guardt &guard,
    optionalt<dstringt> parent = optionalt<dstringt>());

public:
  friend std::ostream &
  operator<<(std::ostream &os, const ls_recursion_childt &child);

  /// creates an unknown variable for each written global and return value
  void assign_written(
    const resolvet &resolve,
    const assign_unknownt &assign_unknown);

  /// creates a new rec child
  ///
  /// \param resolve: used to resolve parameters and read globals variables
  static ls_recursion_childt create(
    size_t id,
    const ls_func_info &info,
    const guardt guard,
    const resolvet &resolve,
    optionalt<dstringt> abstract_parent);
};

/// an abstract recursive function
/// create via the create method and finish it via the assign_written method
class ls_recursion_nodet : public ls_recursion_baset
{
  const guardt current_guard;
  const name_mappingt prev_input_mapping;

public:
  ls_recursion_nodet(
    const ls_func_info &info,
    name_mappingt input,
    name_mappingt prev_input_mapping,
    const guardt &current_guard);

public:
  friend std::ostream &
  operator<<(std::ostream &os, const ls_recursion_nodet &nodet);

  /// resolves the values of written globals and the return value, assigns them unknown values
  /// and assigns the stack
  void assign_written(
    const resolvet &resolve,
    const assign_unknownt &assign_unknown,
    const set_guardt &set_guard);

  const name_mappingt &get_prev_input_mapping() const
  {
    return prev_input_mapping;
  }

  /// creates a new node
  ///
  /// \param assign_unknown: used to create an unknown variable for each read global or parameter
  static ls_recursion_nodet create(
    const ls_func_info &info,
    guardt current_guard,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown,
    const set_guardt &set_guard);

  const guardt &get_prev_guard() const
  {
    return current_guard;
  }
};

/// Stores recursion nodes
class ls_recursion_node_dbt
{
  const ls_infot &info;
  std::unordered_map<dstringt, ls_recursion_nodet> nodes;

  optionalt<ls_recursion_nodet *> unfinished_node;

  std::vector<ls_recursion_childt> rec_children;

public:
  explicit ls_recursion_node_dbt(const ls_infot &info) : info(info)
  {
  }

  void create_rec_child(
    dstringt func_name,
    const guardt &guard,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown)
  {
    assert(enabled());
    create_rec_child(
      func_name,
      guard,
      resolve,
      resolve,
      assign_unknown,
      unfinished_node.has_value()
        ? optionalt<dstringt>{unfinished_node.value()->func_name}
        : optionalt<dstringt>());
  }

protected:
  /// \param resolve: used to resolve parameters and read globals variables
  /// \param assign_unknown: used to create an unknown variable for each written global and return value
  void create_rec_child(
    dstringt func_name,
    const guardt &guard,
    const resolvet &initial_resolve,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown,
    optionalt<dstringt> abstract_parent)
  {
    auto child = ls_recursion_childt::create(
      rec_children.size(),
      info.get_func_info(func_name),
      guard,
      initial_resolve,
      std::move(abstract_parent));
    child.assign_written(resolve, assign_unknown);
    rec_children.push_back(std::move(child));
  }

public:
  bool contains(const dstringt func_name) const
  {
    return nodes.find(func_name) != nodes.end() &&
           (!unfinished_node.has_value() ||
            unfinished_node.value()->func_name == func_name);
  }

  /// Starts an unfinished node
  /// throws a runtime_error if there is already an unfinished node or if a node with this id already exists
  ///
  /// \param resolve: resolves read globals and parameters
  void begin_node(
    dstringt func_name,
    const guardt &current_guard,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown,
    const set_guardt &set_guard)
  {
    assert(enabled());
    check_begin_node(func_name);
    nodes.emplace(
      func_name,
      ls_recursion_nodet::create(
        info.get_func_info(func_name),
        current_guard,
        resolve,
        assign_unknown,
        set_guard));
    unfinished_node = {&nodes.at(func_name)};
  }

  /// finishes the current unfinished node and creates a rec child
  void finish_node(
    dstringt func_name,
    const resolvet &resolve,
    const assign_unknownt &assign_unknown,
    const set_guardt &set_guard)
  {
    assert(func_name == unfinished_node.value()->func_name);
    unfinished_node.value()->assign_written(resolve, assign_unknown, set_guard);
    create_rec_child(
      func_name,
      unfinished_node.value()->get_prev_guard(),
      [&](dstringt var) {
        return unfinished_node.value()->get_prev_input_mapping().at(var);
      },
      resolve,
      assign_unknown,
      {});
    unfinished_node = {};
  }

  friend std::ostream &
  operator<<(std::ostream &os, const ls_recursion_node_dbt &dbt);

  virtual ~ls_recursion_node_dbt()
  {
    if(unfinished_node.has_value())
    {
      std::cerr << "error: unfinished_node with id "
                << unfinished_node.value()->func_name << "\n";
    }
  }

  const std::unordered_map<dstringt, ls_recursion_nodet> &get_nodes() const
  {
    return nodes;
  }

  bool in_abstract_recursion() const
  {
    return unfinished_node.has_value();
  }

  bool enabled() const
  {
    return is_recursion_graph_enabled();
  }

  unsigned int inlining_depth() const
  {
    return recursion_graph_inlining();
  }

private:
  void check_begin_node(dstringt id) const
  {
    std::string id_str = id.c_str();
    if(unfinished_node.has_value())
    {
      throw std::runtime_error(
        "error: unfinished node with id " + id_str + " already exists");
    }
    if(contains(id))
    {
      throw std::runtime_error(
        "error: already contains a node with id " + id_str);
    }
    if(!info.has_func_info(id))
    {
      throw std::runtime_error("error: unknown function " + id_str);
    }
  }
};

#endif //CBMC_LS_REC_GRAPH_H
