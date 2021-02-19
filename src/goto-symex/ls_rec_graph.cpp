#include "ls_rec_graph.h"

#include "loopstack.hpp"
#include <utility>

std::ostream &operator<<(std::ostream &os, const name_mappingt &mapping)
{
  for(auto it = mapping.begin(); it != mapping.end(); it++)
  {
    os << (it != mapping.begin() ? " " : "") << it->first << " " << it->second;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const ls_recursion_childt &child)
{
  return os << "c rec child " << child.id << " " << child.func_name
            << " | input " << child.input << " | output " << child.output
            << " | constraint " << get_guard_variables(child.guard);
}

ls_recursion_childt ls_recursion_childt::create(
  size_t id,
  const ls_func_info &info,
  const guardt &guard,
  const resolvet &resolve,
  const assign_unknownt &assign_unknown)
{
  auto input = create_mapping(info.get_parameters_and_read_globals(), resolve);
  auto output =
    create_mapping(info.get_assigned_globals_and_return(), [&](dstringt var) {
      assign_unknown(var);
      return resolve(var);
    });
  return {id, info, input, output, guard};
}

std::ostream &operator<<(std::ostream &os, const ls_recursion_nodet &node)
{
  return os << "c rec node " << node.func_name << " | input " << node.input
            << " | output " << node.output;
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

void ls_recursion_node_dbt::process_requested(
  const requested_functiont &func,
  const std::function<void()> &state_processor,
  const resolvet &resolve,
  const assign_unknownt &assign_unknown)
{
  in_abstract_processing = true;
  if(nodes.find(func.name()) != nodes.end())
  {
    return;
  }
  auto &func_info = info.get_func_info(func.name());
  auto input = create_mapping(
    func_info.get_parameters_and_read_globals(), [&](dstringt var) {
      assign_unknown(var);
      return resolve(var);
    });
  state_processor();
  auto output =
    create_mapping(func_info.get_assigned_globals_and_return(), resolve);
  ls_recursion_nodet node{func_info, std::move(input), std::move(output)};
  nodes.emplace(func.name(), std::move(node));
  in_abstract_processing = false;
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
    return std::stoul(opt);
  }
  return 0;
}

std::string to_string(const name_mappingt &mapping)
{
  std::stringstream ss;
  ss << mapping;
  return ss.str();
}

dstringt basename(const dstringt &name)
{
  std::string name_str{name.c_str()};
  auto middle = name_str.find('#');
  return name_str.substr(0, middle);
}

dstringt l0_name(const dstringt &name)
{
  std::string name_str{name.c_str()};
  auto middle = name_str.find('!');
  return name_str.substr(0, middle);
}

std::tuple<dstringt, size_t> split_var(const dstringt &name)
{
  std::string name_str{name.c_str()};
  auto middle = name_str.find('#');
  std::istringstream iss(name_str.substr(middle + 1));
  size_t num;
  iss >> num;
  return {name_str.substr(0, middle), num};
}

bool is_guard(dstringt var)
{
  return strstr(var.c_str(), "::\\guard") != nullptr;
}

bool is_oa_constant(dstringt var)
{
  return strstr(var.c_str(), "oa_constant::") != nullptr;
}

bool should_fully_over_approximate(dstringt function_id)
{
  return strstr(function_id.c_str(), "__CPROVER__start") != nullptr;
}
