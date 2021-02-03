//
// Created by bechberger-local on 11.12.20.
//

#include "loopstack.hpp"
#include "analyses/guard_expr.h"
#include <cstring>
#include <expr_iterator.h>
#include <string_utils.h>
#include <symbol.h>
#include <utility>

bool is_guard(dstringt var)
{
  return strstr(var.c_str(), "::\\guard#") != nullptr;
}

variablest variablest::restrict_to(const variablest &other) const
{
  variablest new_variables;
  for(const auto &item : other.vars)
  {
    auto it = vars.find(item.first);
    if(it != vars.end())
    {
      // guard variables and constant variables are thereby excluded
      new_variables.vars[item.first] = vars.find(item.first)->second;
    }
  }
  return new_variables;
}

optionalt<std::string> variablest::get_last_guard() const
{
  if(contains_guard())
  {
    return std::string("symex_goto::guard") +
           std::to_string(*vars.find("symex_goto::guard")->second.rbegin());
  }
  return {};
}

std::ostream &operator<<(std::ostream &os, const variablest &variables)
{
  os << "VariableSet(";
  for(auto iter : variables.vars)
  {
    os << " " << iter.first << ": ";
    for(const auto &num : iter.second)
    {
      os << num << " ";
    }
  }
  return os << ")";
}

void scope::assign(dstringt var)
{
  assert(!guard || !split_before(var));
  if(is_guard(var))
  {
    guard = var;
  }
  assigned.emplace(var);
  variables.insert(var);
}

bool scope::split_before(dstringt var) const
{
  return is_guard(var);
}

bool scope::matches_guard(dstringt guard_var) const
{
  return guard && guard.value() == guard_var;
}

void scope::read(dstringt var)
{
  if(getenv("LOG_READ"))
  {
    std::cerr << "read " << var << "\n";
  }
  read_variables.emplace(var);
}

guard_variablest get_guard_variables(const guard_exprt &guard, size_t omit_last)
{
  if(guard.is_true())
  {
    return {};
  }
  assert(!guard.is_false());

  auto func = [](exprt expr) {
    assert(!expr.is_constant());
    if(expr.id() == ID_not)
    {
      return std::make_tuple(
        to_symbol_expr(to_not_expr(expr).op()).get_identifier(), false);
    }
    return std::make_tuple(to_symbol_expr(expr).get_identifier(), true);
  };

  auto expr = guard.as_expr();
  if(expr.id() != ID_and)
  {
    return {func(expr)};
  }
  std::vector<std::tuple<dstringt, bool>> ret;
  auto ops = to_and_expr(expr).operands();
  PRECONDITION(omit_last < ops.size());
  std::transform(
    ops.begin(), ops.end() - omit_last, std::back_inserter(ret), func);
  return ret;
}

std::ostream &
operator<<(std::ostream &os, const guard_variablest &guard_variables)
{
  for(auto it = guard_variables.begin(); it != guard_variables.end(); it++)
  {
    os << (it != guard_variables.begin() ? " " : "")
       << (std::get<1>(*it) ? "" : "-") << std::get<0>(*it);
  }
  return os;
}

std::vector<dstringt> loop_iteration::assigned_variables() const
{
  return loop->get_stack()->variables(start_scope, end_scope);
}

loop_iteration::loop_iteration(
  size_t id,
  loopt *loop,
  size_t start_scope,
  size_t end_scope,
  const bool is_second_to_last_iteration,
  const bool is_last_iteration)
  : id(id),
    loop(loop),
    start_scope(start_scope),
    end_scope(end_scope),
    is_second_to_last_iteration(is_second_to_last_iteration),
    is_last_iteration(is_last_iteration)
{
  assert(!is_second_to_last_iteration || !is_last_iteration);
}

void loopt::add_used_after(dstringt var)
{
  if(getenv("LOG_LOOP_MERGE") != nullptr || getenv("LOG_LOOP"))
  {
    std::cerr << "loop" << id << " use after " << var << "\n";
  }
  used_after.emplace(var);
  used_after_variables.insert(var);
}
bool loop_iteration::contains_variables() const
{
  for(size_t i = start_scope; i <= end_scope; i++)
  {
    auto sc = loop->get_scope(i);
    if(!loop->get_scope(i).assigned.empty())
    {
      return true;
    }
  }
  return false;
}

variablest loop_iteration::assigned_variable_set() const
{
  auto vec = loop->get_stack()->variables(start_scope, end_scope);
  return {{vec.begin(), vec.end()}};
}

optionalt<dstringt> loop_iteration::guard() const
{
  assert(id > 0);
  for(size_t i = end_scope; i >= start_scope; i--)
  {
    auto sc = loop->get_scope(i);
    if(sc.variables.contains_guard())
    {
      optionalt<dstringt> ret;
      for(const auto &var : sc.assigned)
      {
        ret = var;
      }
      if(ret)
      {
        return ret;
      }
    }
  }
  return {};
}

size_t loopt::adjusted_end_scope() const
{
  size_t tmp_end = this->before_end_scope;
  auto first_loop_guard = first_guard();
  if(first_loop_guard)
  {
    while(
      tmp_end > 0 &&
      !stack->get_scope(tmp_end + 1).matches_guard(first_loop_guard.value()))
    {
      tmp_end--;
    }
  }
  return tmp_end;
}

const scope &loopt::get_scope(size_t scope_id) const
{
  return stack->get_scope(scope_id);
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
  os << "c loop " << loop.id << " " << loop.func_id << " " << loop.nr;
  os << " | input";
  for(const auto &var : loop.outer_loop_variables())
  {
    os << " " << var;
  }
  os << " | guards ";
  if(!loop.guards.empty())
  {
    os << get_guard_variables(loop.guards.back(), 1);
  }
  os << " | lguard ";
  if(!loop.guards.empty())
  {
    os << loop.guards.back().last_guard().to_string2();
  }
  os << " | linput";
  for(const auto &var : loop.last_iter_input())
  {
    os << " " << var;
  }
  os << " | loutput";
  for(const auto &var : loop.last_iter_output())
  {
    os << " " << var;
  }
  os << " | output";
  // dtodo: output and loutput should be the same, but they aren't apparently…
  for(const auto &var : loop.result_variables())
  {
    os << " " << var;
  }
  return os;
}

loopt::loopt(
  size_t id,
  const dstringt func_id,
  size_t nr,
  loop_stackt *stack,
  const size_t depth,
  guardt context_guard,
  size_t before_end_scope)
  : id(id),
    func_id(func_id),
    nr(nr),
    stack(stack),
    depth(depth),
    context_guard(context_guard),
    before_end_scope(before_end_scope)
{
}

void loopt::push_iteration(
  size_t end_scope_of_last,
  size_t start_scope,
  bool is_second_to_last_iteration,
  bool is_last_iteration)
{
  if(!iterations.empty())
  {
    assert(iterations.back().end_scope == 0);
    iterations.back().end_scope = end_scope_of_last;
  }
  iterations.push_back(loop_iteration(
    iterations.size(),
    this,
    start_scope,
    0,
    is_second_to_last_iteration,
    is_last_iteration));
}

void loopt::end_loop(size_t end_scope)
{
  assert(iterations.back().end_scope == 0);
  iterations.back().end_scope = end_scope;
  assert(!iterations.back().contains_variables());
  iterations.pop_back();
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

std::vector<dstringt> loopt::outer_loop_variables() const
{
  return stack->variables(0, adjusted_end_scope());
}

std::vector<std::string> loopt::last_iter_output() const
{
  return iterations.rbegin()
    ->assigned_variable_set()
    .restrict_to(used_after_variables)
    .get_last();
}

std::vector<std::string> loopt::last_iter_input() const
{
  return iterations.at(iterations.size() - 2)
    .assigned_variable_set()
    .restrict_to(used_after_variables)
    .get_last();
}

std::vector<std::string> loopt::result_variables() const
{
  return used_after_variables
    .restrict_to(iterations.rbegin()->assigned_variable_set())
    .get_first();
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
      guard_symbol_to_loop.emplace(to_symbol_expr(guard.last_guard()), last.id);
    }
  }
}

loopt *loop_stackt::get_loop_for_guard_symbol(exprt guard_expr)
{
  auto symbol = to_symbol_expr(guard_expr);

  if(guard_symbol_to_loop.find(symbol) == guard_symbol_to_loop.end())
  {
    return nullptr;
  }
  return &loops.at(guard_symbol_to_loop.at(symbol));
}

void loop_stackt::emit(std::ostream &os)
{
  assert(is_initialized);
  for(const auto &loop : loops)
  {
    os << loop;
  }
  os << abstract_recursion();
}

void loop_stackt::assign(dstringt id)
{
  PRECONDITION(is_initialized);
  PRECONDITION(!id.empty());
  if(getenv("LOG_LOOP") != nullptr)
  {
    std::cerr << " assign " << id << "\n";
  }

  if(current_scope().split_before(id))
  {
    push_back_scope();
  }
  current_scope().assign(id);
}

bool loop_stackt::should_discard_assignments_to(const dstringt &lhs)
{
  return is_in_loop() && current_loop().in_second_to_last_iteration() &&
         make_second_to_last_iteration_abstract();
}
