//
// Created by bechberger-local on 26.01.21.
//

#include "ls_rec_graph.h"

#include "loopstack.hpp"
#include <utility>

name_mappingt create_mapping(
  const std::unordered_set<dstringt> &variables,
  const std::function<dstringt(dstringt)> &resolve)
{
  name_mappingt ret;
  for(auto var : variables)
  {
    ret.emplace(var, resolve(var));
  }
  return ret;
}

std::ostream &operator<<(std::ostream &os, const name_mappingt &mapping)
{
  for(auto it = mapping.begin(); it != mapping.end(); it++)
  {
    os << (it != mapping.begin() ? " " : "") << it->first << " " << it->second;
  }
  return os;
}

std::unordered_set<dstringt> ls_recursion_baset::get_written() const
{
  return info.get_assigned_globals_and_return();
}

std::ostream &operator<<(std::ostream &os, const ls_recursion_childt &child)
{
  if(child.parent)
  {
    os << "c parent of " << child.id << " is " << *child.parent << "\n";
  }
  return os << "c rec child " << child.id << " " << child.func_name
            << " | input " << child.input << " | output " << child.output
            << " | constraint " << get_guard_variables(child.guard);
}

ls_recursion_childt::ls_recursion_childt(
  size_t id,
  const ls_func_info &info,
  name_mappingt input,
  const guardt &guard,
  optionalt<dstringt> parent)
  : ls_recursion_baset(info, std::move(input)),
    id(id),
    guard(guard),
    parent(std::move(parent))
{
}

void ls_recursion_childt::assign_written(
  const resolvet &resolve,
  const assign_unknownt &assign_unknown)
{
  for(auto var : get_written())
  {
    assign_unknown(var);
    output.emplace(var, resolve(var));
  }
}

ls_recursion_childt ls_recursion_childt::create(
  size_t id,
  const ls_func_info &info,
  const guardt guard,
  const resolvet &resolve,
  optionalt<dstringt> abstract_parent)
{
  return {
    id,
    info,
    create_mapping(info.get_parameters_and_read_globals(), resolve),
    guard,
    abstract_parent};
}

std::ostream &operator<<(std::ostream &os, const ls_recursion_nodet &node)
{
  return os << "c rec node " << node.func_name << " | input " << node.input
            << " | output " << node.output;
}

ls_recursion_nodet::ls_recursion_nodet(
  const ls_func_info &info,
  name_mappingt input,
  name_mappingt prev_input_mapping,
  const guardt &current_guard)
  : ls_recursion_baset(info, std::move(input)),
    current_guard(current_guard),
    prev_input_mapping(std::move(prev_input_mapping))
{
}

void ls_recursion_nodet::assign_written(
  const resolvet &resolve,
  const assign_unknownt &assign_unknown,
  const set_guardt &set_guard)
{
  for(auto var : get_written())
  {
    output.emplace(var, resolve(var));
    assign_unknown(var);
  }
  set_guard(current_guard);
}

ls_recursion_nodet ls_recursion_nodet::create(
  const ls_func_info &info,
  const guardt cur_guard,
  const resolvet &resolve,
  const assign_unknownt &assign_unknown,
  const set_guardt &set_guard)
{
  true_exprt true_expr;
  guard_expr_managert manager; // not used in the implementation
  set_guard(guardt{true_expr, manager});
  auto prev = create_mapping(info.get_parameters_and_read_globals(), resolve);
  return {
    info,
    create_mapping(
      info.get_parameters_and_read_globals(),
      [&](dstringt var) {
        assign_unknown(var);
        return resolve(var);
      }),
    prev,
    cur_guard};
}

std::ostream &operator<<(std::ostream &os, const ls_recursion_node_dbt &dbt)
{
  for(const auto &it : dbt.get_nodes())
  {
    os << it.second << "\n";
  }
  for(const auto &child : dbt.rec_children)
  {
    os << child << "\n";
  }
  return os;
}

bool is_recursion_graph_enabled()
{
  return getenv("ENABLE_REC_GRAPH") || getenv("REC_GRAPH_INLINING");
}

unsigned int recursion_graph_inlining()
{
  char *opt = getenv("REC_GRAPH_INLINING");
  if(opt && strlen(opt) > 0)
  {
    return std::stoi(opt);
  }
  return 0;
}

std::string to_string(const name_mappingt &mapping)
{
  std::stringstream ss;
  ss << mapping;
  return ss.str();
}
