//
// Created by bechberger-local on 11.12.20.
//

#include "loopstack.hpp"
#include "analyses/guard_expr.h"
#include <cstring>
#include <expr_iterator.h>
#include <string_utils.h>
#include <symbol.h>

bool is_guard(dstringt var)
{
  return strstr(var.c_str(), "::\\guard#1") != nullptr;
}

void scope::assign(dstringt var)
{
  assert(!guard || !split_before(var));
  if(is_guard(var))
  {
    guard = var;
  }
  assigned.emplace(var);
}

bool scope::split_before(dstringt var) const
{
  return is_guard(var);
}

bool scope::matches_guard(dstringt guard_var) const
{
  return guard && guard.value() == guard_var;
}

std::vector<dstringt> loop_iteration::assigned_variables() const
{
  return stack->variables(start_scope, end_scope);
}

std::vector<dstringt> loop_iteration::outer_loop_variables() const
{
  return stack->variables(0, adjusted_end_scope());
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

loop_iteration::loop_iteration(
  size_t id,
  loop_stack *stack,
  size_t before_end_scope,
  size_t start_scope,
  size_t end_scope)
  : id(id),
    stack(stack),
    before_end_scope(before_end_scope),
    start_scope(start_scope),
    end_scope(end_scope)
{
}

bool loop_iteration::is_first_iter_guard(dstringt var)
{
  return guard &&
         to_symbol_expr(guard.value().first_guard()).get_identifier() == var;
}

size_t loop_iteration::adjusted_end_scope() const
{
  size_t tmp_end = this->before_end_scope;
  auto first_loop_guard =
    to_symbol_expr(guard.value().first_guard()).get_identifier();
  while(tmp_end >= 0 &&
        !stack->get_scope(tmp_end + 1).matches_guard(first_loop_guard))
  {
    tmp_end--;
  }
  return tmp_end;
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
  for(const auto &relation : relations)
  {
    os << "c relate";
    for(const auto &part : relation)
    {
      os << " " << part;
    }
    os << "\n";
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
  if(getenv("LOG_LOOP") != nullptr)
  {
    std::cerr << " assign " << id << "\n";
  }
  /*if (current_recursion){
    std::cerr << " current_recursion";
  }
  if (current_recursion_waits_for_return && contains_tmp){
    std::cerr << " cr_waits_for_return";
  }*/
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
    if(current_scope().split_before(id))
    {
      push_back_scope();
    }
    current_scope().assign(id);
  }
}

bool loop_stack::should_discard_assignments_to(const dstringt &lhs)
{
  return current_recursion && current_recursion_waits_for_return &&
         std::string(lhs.c_str()).find("::$tmp::return_value") !=
           std::string::npos;
}

void loop_stack::relate(std::vector<dstringt> symbols, exprt expr)
{
  std::vector<dstringt> rel = symbols;
  for(auto it = expr.depth_begin(); it != expr.depth_end(); ++it)
  {
    if(it->id() == ID_symbol)
    {
      rel.push_back(to_symbol_expr(*it).get_identifier());
    }
  }
  relations.push_back(std::move(rel));
}
