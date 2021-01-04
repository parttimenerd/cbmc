//
// Created by bechberger-local on 11.12.20.
//

#include "loopstack.hpp"
#include "analyses/guard_expr.h"
#include <string_utils.h>

void scope::assign(dstringt var)
{
  assigned.emplace(var);
}

std::vector<dstringt> loop_iteration::assigned_variables() const
{
  return stack->variables(start.id, end.id);
}

std::string normalize(std::string str)
{
  auto split = split_string(str, '@', true, true);
  if(split.size() == 1)
  {
    return str;
  }
  auto split2 = split_string(split.at(1), '#', true, true);
  return split.at(0);
}

std::unordered_set<std::string> normalize(std::vector<dstringt> vars)
{
  std::unordered_set<std::string> ret;
  for(auto var : vars)
  {
    ret.emplace(normalize(var.c_str()));
  }
  return ret;
}

std::unordered_map<std::string, std::vector<dstringt>>
sorted_hash(std::vector<dstringt> &vec)
{
  std::unordered_map<std::string, std::vector<dstringt>> ret;
  for(auto var : vec)
  {
    auto split = split_string(split_string(var.c_str(), '#').at(0), '@');
    ret[split.at(0)].emplace_back(var);
  }
  return ret;
}

std::vector<dstringt> loop_iteration::outer_loop_variables() const
{
  auto outer_vars = stack->variables(0, before_end.id);
  auto assigned_vars = assigned_variables();
  auto normalized_assigned = normalize(assigned_vars);

  std::vector<dstringt> ret;

  auto outer_sorted = sorted_hash(outer_vars);
  auto assigned_sorted = sorted_hash(assigned_vars);

  for(const auto &item : outer_sorted)
  {
    if(assigned_sorted.find(item.first) != assigned_sorted.end())
    {
      auto assigned_size = assigned_sorted[item.first].size();
      if(assigned_size * 2 < item.second.size())
      {
        for(size_t i = 0; i < item.second.size() - assigned_size * 2; i++)
        {
          ret.emplace_back(item.second.at(i));
        }
      }
    }
    else
    {
      ret.insert(ret.end(), item.second.begin(), item.second.end());
    }
  }

  /*for(auto var : outer_vars)
  {
    if (normalized_assigned.find(normalize(var.c_str())) == assigned_vars.end()){
      ret.emplace_back(var);
    }
  }*/

  return ret;
}

std::ostream &operator<<(std::ostream &os, const loop_iteration &iteration)
{
  os << "c loop " << iteration.id << " assigned";
  for(const auto &var : iteration.used_after)
  {
    os << " " << var;
  }
  os << " | outer";
  for(const auto &var : iteration.outer_loop_variables())
  {
    os << " " << var;
  }
  os << "\n";
  return os;
}

bool aborted_recursion::assign_return(dstringt id)
{
  //std::cerr << "return assigned " << id << " " << parameters.begin()->c_str() << "\n";
  std::string str = id.c_str();
  if(
    !return_var && str.find(func_id) == 0 &&
    (str.rfind("::return_value") != std::string::npos ||
     str.rfind("#return_value") != std::string::npos))
  {
    return_var = id;
    return true;
  }
  return false;
}

bool loop_iteration::set_iter_guard(guard_exprt &new_guard)
{
  if(!has_iter_guard())
  {
    std::cout << " new guard found " << new_guard.as_expr().to_string2()
              << "\n";
    guard = new_guard;
    return true;
  }
  return false;
}

std::ostream &operator<<(std::ostream &os, const aborted_recursion &recursion)
{
  if(!recursion.return_var)
  {
    return os;
  }
  os << "c recursion return " << *recursion.return_var;
  os << " | param";
  for(const auto &var : recursion.parameters)
  {
    os << " " << var;
  }
  os << "\n";
  return os;
}

std::vector<dstringt>
loop_stack::variables(size_t start_scope, size_t end_scope)
{
  std::vector<dstringt> ret;
  for(size_t i = start_scope; i <= end_scope; i++)
  {
    auto sc = scopes.at(i);
    for(auto var : sc.assigned)
    {
      ret.push_back(var);
    }
  }
  return ret;
}

void loop_stack::set_iter_guard(guard_exprt &guard)
{
  if(!iteration_stack.empty())
  {
    auto &last = iterations.at(iteration_stack.back());
    if(last.set_iter_guard(guard))
    {
      first_guard_to_iter.emplace(guard.first_guard(), last.id);
      last_guard_to_iter.emplace(guard.last_guard(), last.id);
    }
  }
}

loop_iteration *loop_stack::get_iter_for_first_guard(exprt guard_expr)
{
  if(first_guard_to_iter.find(guard_expr) == first_guard_to_iter.end())
  {
    return nullptr;
  }
  return &iterations.at(first_guard_to_iter.at(guard_expr));
}

loop_iteration *loop_stack::get_iter_for_last_guard(exprt guard_expr)
{
  if(last_guard_to_iter.find(guard_expr) == last_guard_to_iter.end())
  {
    return nullptr;
  }
  return &iterations.at(last_guard_to_iter.at(guard_expr));
}

void loop_stack::emit(std::ostream &os)
{
  for(const auto &iteration : iterations)
  {
    os << iteration;
  }
  for(const auto &recursion : recursions)
  {
    os << recursion;
  }
}

void loop_stack::assign(dstringt id)
{
  if(id.empty())
  {
    throw "empty var";
  }
  bool contains_tmp =
    std::string(id.c_str()).find("::$tmp::return_value") != std::string::npos;
  /*std::cerr << " assign " << id;
  if (current_recursion){
    std::cerr << " current_recursion";
  }
  if (current_recursion_waits_for_return){
    std::cerr << " cr_waits_for_return";
  }
  if (contains_tmp){
    std::cerr << " contains $tmp";
  }
  std::cerr << "\n";*/
  if(current_recursion)
  {
    if(current_recursion_waits_for_return)
    {
      if(contains_tmp)
      {
        current_recursion->assign_return(id);
        current_scope().assign(id);
        current_recursion_waits_for_return = false;
        recursions.emplace_back(std::move(*current_recursion));
        current_recursion = {};
      }
    }
    else
    {
      current_recursion->assign(id);
    }
  }
  else
  {
    current_scope().assign(id);
  }
}
bool loop_stack::should_discard_assignments_to(const dstringt &lhs)
{
  return current_recursion && current_recursion_waits_for_return &&
         std::string(lhs.c_str()).find("::$tmp::return_value") !=
           std::string::npos;
}
