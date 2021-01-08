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
  return loop->get_stack()->variables(start_scope, end_scope);
}

std::ostream &operator<<(std::ostream &os, const loop_iteration &iteration)
{
  os << "c loop " << iteration.loop->id << " assigned";
  for(const auto &var : iteration.used_after)
  {
    os << " " << var;
  }
  os << " | outer";
  for(const auto &var : iteration.loop->outer_loop_variables())
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
  loopt *loop,
  size_t start_scope,
  size_t end_scope,
  const bool is_last_iteration)
  : id(id),
    loop(loop),
    start_scope(start_scope),
    end_scope(end_scope),
    is_second_to_last_iteration(is_last_iteration)
{
}

bool loop_iteration::is_first_iter_guard(dstringt var)
{
  return guard &&
         to_symbol_expr(guard.value().first_guard()).get_identifier() == var;
}

void loop_iteration::add_used_after(dstringt var)
{
  if(getenv("LOG_LOOP_MERGE") != nullptr || getenv("LOG_LOOP"))
  {
    std::cerr << "loop" << loop->id << " " << id << " use after " << var
              << "\n";
  }
  used_after.emplace(var);
}

bool loop_iteration::is_last_iteration() const
{
  return loop->iterations.size() > 1 && id > 0 &&
         loop->iterations.at(id - 1).is_second_to_last_iteration;
}

size_t loopt::adjusted_end_scope() const
{
  size_t tmp_end = this->before_end_scope;
  auto first_loop_guard = first_guard();
  while(tmp_end >= 0 &&
        !stack->get_scope(tmp_end + 1).matches_guard(first_loop_guard))
  {
    tmp_end--;
  }
  return tmp_end;
}

const scope &loopt::get_scope(size_t scope_id) const
{
  return stack->get_scope(id);
}

const loop_stackt *loopt::get_stack() const
{
  return stack;
}

std::ostream &operator<<(std::ostream &os, const loopt &loop)
{
  for(const auto &relation : loop.relations)
  {
    os << "c relate";
    for(const auto &part : relation)
    {
      os << " " << part;
    }
    os << "\n";
  }
  return os << loop.back();
}

loopt::loopt(
  size_t id,
  loop_stackt *stack,
  const size_t depth,
  size_t before_end_scope)
  : id(id), stack(stack), depth(depth), before_end_scope(before_end_scope)
{
}

void loopt::push_loop(
  size_t end_scope_of_last,
  size_t start_scope,
  bool is_last_iteration)
{
  if(!iterations.empty())
  {
    assert(iterations.back().end_scope == 0);
    iterations.back().end_scope = end_scope_of_last;
  }
  iterations.push_back(
    loop_iteration(iterations.size(), this, start_scope, 0, is_last_iteration));
}

void loopt::end_last_iteration(size_t end_scope)
{
  assert(iterations.back().end_scope == 0);
  iterations.back().end_scope = end_scope;
}

void loopt::add_guard(guard_exprt &iter_guard)
{
  guards.push_back(iter_guard);
}

void loopt::relate(std::vector<dstringt> symbols, exprt expr)
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

std::vector<dstringt> loopt::outer_loop_variables()
{
  return stack->variables(0, adjusted_end_scope());
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
loop_stackt::variables(size_t start_scope, size_t end_scope) const
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

void loop_stackt::set_iter_guard(guard_exprt &guard)
{
  if(!loop_stack.empty())
  {
    auto &last = current_loop();
    if(!guard.is_true() && !guard.is_false())
    {
      //std::cerr << "# set iter guard " << guard.as_expr().to_string2() << " to loop " << last.id << "\n";
      last.add_guard(guard);
      last_guard_to_loop.emplace(guard.last_guard(), last.id);
    }
  }
}

loopt *loop_stackt::get_iter_for_last_guard(exprt guard_expr)
{
  if(last_guard_to_loop.find(guard_expr) == last_guard_to_loop.end())
  {
    return nullptr;
  }
  return &loops.at(last_guard_to_loop.at(guard_expr));
}

void loop_stackt::emit(std::ostream &os)
{
  for(const auto &loop : loops)
  {
    os << loop;
  }
  for(const auto &recursion : recursions)
  {
    os << recursion;
  }
}

void loop_stackt::assign(dstringt id)
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

bool loop_stackt::should_discard_assignments_to(const dstringt &lhs)
{
  return current_recursion && current_recursion_waits_for_return &&
         std::string(lhs.c_str()).find("::$tmp::return_value") !=
           std::string::npos;
}
