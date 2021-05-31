//
// Created by bechberger-local on 11.12.20.
//

#include "loopstack.hpp"
#include "analyses/guard_expr.h"
#include <cstring>
#include <expr_iterator.h>
#include <string_utils.h>

std::ostream &operator<<(std::ostream &os, const variablest &variables)
{
  os << "VariableSet(";
  for(auto &iter : variables.var_map)
  {
    os << " " << iter.first << ": ";
    for(const auto &num : iter.second)
    {
      os << num << " ";
    }
  }
  return os << ")";
}

std::unordered_map<dstringt, size_t> variablest::get_first() const
{
  std::unordered_map<dstringt, size_t> ret;
  for(const auto &item : var_map)
  {
    if(!item.second.empty())
    {
      ret.emplace(item.first, *item.second.begin());
    }
  }
  return ret;
}

void variablest::insert(dstringt var)
{
  auto split_res = split_var(var);
  var_map[std::get<0>(split_res)].insert(std::get<1>(split_res));
  variables.emplace(var);
}

std::vector<dstringt> variablest::get_var_bases_not_in(
  const std::unordered_map<dstringt, size_t> &&vars) const
{
  std::vector<dstringt> ret;
  for(const auto &it : var_map)
  {
    auto &base = it.first;
    auto vars_it = vars.find(base);
    if(
      vars_it == vars.end() ||
      it.second.find(vars_it->second) == it.second.end())
    {
      ret.push_back(base);
    }
  }
  return ret;
}

std::vector<dstringt> variablest::get_var_bases() const
{
  std::vector<dstringt> ret;
  for(const auto &it : var_map)
  {
    ret.push_back(it.first);
  }
  return ret;
}

void scopet::assign(dstringt var)
{
  assert(!guard || !split_before(var));
  if(is_guard(var))
  {
    guard = var;
  }
  assigned.emplace(var);
  access(
    var); // dtodo: only necessary as not all reads are registered… Don't ask…
}

bool scopet::split_before(dstringt var) const
{
  return is_guard(var);
}

bool scopet::matches_guard(dstringt guard_var) const
{
  return guard && guard.value() == guard_var;
}

void scopet::access(dstringt var)
{
  if(getenv("LOG_ACCESS"))
  {
    std::cerr << "access " << var << "\n";
  }
  accessed.emplace(var);
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

loop_iterationt::loop_iterationt(
  size_t id,
  loopt *loop,
  size_t start_scope,
  size_t end_scope)
  : id(id), loop(loop), start_scope(start_scope), end_scope(end_scope)
{
}

bool loop_iterationt::contains_variables() const
{
  for(size_t i = start_scope; i <= end_scope; i++)
  {
    auto sc = loop->get_scope(i);
    if(!loop->get_scope(i).get_assigned().empty())
    {
      return true;
    }
  }
  return false;
}

const loop_iter_variablest &loop_iterationt::get_variables() const
{
  if(!variables)
  {
    variables = loop_iter_variablest{
      variablest{loop->get_stack()->variables(
        start_scope,
        end_scope,
        [](const scopet &sc) { return sc.get_accessed(); })},
      variablest{loop->get_stack()->variables(start_scope, end_scope)}};
  }
  return variables.value();
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

const scopet &loopt::get_scope(size_t scope_id) const
{
  return stack->get_scope(scope_id);
}

const loop_stackt *loopt::get_stack() const
{
  return stack;
}

void output_nm(std::ostream &os, const name_mappingt &mapping)
{
  for(const auto &it : mapping)
  {
    os << " " << it.first << " " << it.second;
  }
}

std::ostream &operator<<(std::ostream &os, const loopt &loop)
{
  if(!loop.last_loop_iter)
  {
    return os;
  }
  os << "c loop " << loop.id << " " << loop.func_id << " " << loop.nr << " "
     << (loop.parent_loop_id.has_value()
           ? std::to_string(loop.parent_loop_id.value())
           : "-1");
  os << " | sfoa " << loop.should_fully_over_approximate;
  os << " | guards";
  if(!loop.guards.empty())
  {
    os << " " << get_guard_variables(loop.guards.back(), 1);
  }
  os << " | lguard";
  if(!loop.guards.empty())
  {
    os << " " << loop.guards.back().last_guard().to_string2();
  }
  auto &last_iter = loop.last_loop_iter;
  os << " | linput";
  output_nm(os, last_iter->get_input());
  os << " | lmisc_input";
  output_nm(os, last_iter->get_misc_input());
  os << " | linner_input";
  output_nm(os, last_iter->get_inner_input());
  os << " | linner_output";
  output_nm(os, last_iter->get_inner_output());
  os << " | loutput";
  output_nm(os, last_iter->get_output());
  return os;
}

void loopt::push_iteration(size_t end_scope_of_last, size_t start_scope)
{
  if(!iterations.empty())
  {
    assert(iterations.back()->get_end_scope() == 0);
    iterations.back()->set_end_scope(end_scope_of_last);
  }
  iterations.emplace_back(
    util_make_unique<loop_iterationt>(iterations.size(), this, start_scope, 0));
}

void loopt::end_loop(size_t end_scope)
{
  assert(iterations.back()->get_end_scope() == 0);
  iterations.back()->set_end_scope(end_scope);
  assert(!iterations.back()->contains_variables());
  iterations.pop_back();
}

void loopt::add_guard(guard_exprt &iter_guard)
{
  guards.push_back(iter_guard);
}

void loopt::begin_last_loop_iteration(
  const resolvet &resolve,
  const assign_unknownt &assign_unknown)
{
  PRECONDITION(iterations.size() >= 2);
  PRECONDITION(!last_loop_iter);
  auto input = get_loop_iter_input();
  std::vector<dstringt> new_input;
  for(const auto &var : std::get<0>(input))
  {
    if(
      !is_guard(var) && (std::get<1>(split_var(resolve(l0_name(var)))) > 0 ||
                         is_oa_constant(resolve(l0_name(var)))))
    {
      new_input.push_back(var);
    }
    else
    {
      std::get<1>(input).push_back(var);
    }
  }
  std::get<0>(input) = new_input;
  auto outer_input = create_mapping(
    std::get<0>(input),
    [&](dstringt var) { return resolve(l0_name(var)); },
    true);
  auto misc_input = create_mapping(
    std::get<1>(input),
    [&](dstringt var) { return resolve(l0_name(var)); },
    true);
  auto inner_input = create_mapping(
    std::get<0>(input),
    [&](dstringt var) {
      assign_unknown(l0_name(var));
      return resolve(l0_name(var));
    },
    true);
  /*std::cerr << "*********************+\n";
  std::cerr << "misc_input  " << misc_input << "\n";
  std::cerr << "outer_input " << outer_input << "\n";
  std::cerr << "inner_input " << inner_input << "\n";*/
  last_loop_iter = {{&back(), outer_input, inner_input, misc_input}};
}

void loopt::end_loop(
  const resolvet &resolve,
  const assign_unknownt &assign_unknown)
{
  PRECONDITION(last_loop_iter.has_value());
  last_loop_iter->set_guard(back().get_guard());
  auto output = get_loop_iter_output();
  auto inner_output = create_mapping(
    output, [&](dstringt var) { return resolve(l0_name(var)); }, true);
  auto outer_output = create_mapping(
    output,
    [&](dstringt var) {
      assign_unknown(l0_name(var));
      return resolve(l0_name(var));
    },
    true);
  last_loop_iter->set_output(inner_output, outer_output);
}

optionalt<dstringt> loopt::first_guard() const
{
  if(has_guards())
  {
    return {to_symbol_expr(guards.front().last_guard()).get_identifier()};
  }
  return {};
}

void loopt::process_assigned_guard_var(dstringt var)
{
  if(!iterations.empty() && !iterations.back()->get_guard())
  {
    iterations.back()->set_guard(var);
  }
}

std::tuple<std::vector<dstringt>, std::vector<dstringt>>
loopt::get_loop_iter_input() const
{
  auto &vars = first_iteration().get_variables();
  auto &all_read = vars.get_accessed();
  auto &written = vars.get_written();
  std::vector<dstringt> input;
  std::vector<dstringt> misc_input;
  for(const auto &var : all_read.get_var_bases())
  {
    if(written.contains(var))
    {
      input.push_back(var);
    }
    else
    {
      misc_input.push_back(var);
    }
  }
  return {input, misc_input};
}

std::vector<dstringt> loop_stackt::variables(
  size_t start_scope,
  size_t end_scope,
  const std::function<std::unordered_set<dstringt>(const scopet &)> &accessor)
  const
{
  std::vector<dstringt> ret;
  for(size_t i = start_scope; i <= end_scope; i++)
  {
    auto sc = scopes.at(i);
    for(auto var : accessor(sc))
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
    }
  }
}

void loop_stackt::emit(std::ostream &os)
{
  if(!is_initialized)
  {
    std::cerr << "ls_stack not properly initialized, omitting output\n";
    return;
  }
  for(const auto &loop : loops)
  {
    os << *loop << "\n";
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
  if(is_in_loop() && is_guard(id))
  {
    current_loop().process_assigned_guard_var(id);
  }
}

void loop_stackt::access(dstringt id)
{
  PRECONDITION(is_initialized);
  PRECONDITION(!id.empty());
  current_scope().access(id);
}

void loop_stackt::push_back_loop(
  dstringt func_id,
  dstringt calling_location,
  size_t loop_nr,
  const guard_exprt &context_guard)
{
  PRECONDITION(is_initialized);
  auto before_id = current_scope().id;
  push_back_scope();
  auto parent = !loop_stack.empty() ? optionalt<size_t>(loop_stack.back())
                                    : optionalt<size_t>();
  loops.emplace_back(util_make_unique<loopt>(
    loops.size(),
    func_id,
    loop_nr,
    parent,
    this,
    loop_stack.size(),
    context_guard,
    before_id,
    func_id
      .empty() /*should_fully_over_approximate(calling_location)*/)); // dtodo: improve
  loop_stack.push_back(loops.back()->id);
  if(getenv("LOG_LOOP") != nullptr)
  {
    std::cerr << "start loop " << current_loop().id << "\n";
  }
}

loopt &loop_stackt::push_loop_iteration()
{
  PRECONDITION(is_initialized);
  if(getenv("LOG_LOOP") != nullptr)
  {
    std::cerr << "push iteration of loop " << current_loop().id;
    std::cerr << "\n";
  }
  push_back_scope();
  current_loop().push_iteration(current_scope().id - 1, current_scope().id);
  return current_loop();
}

void loop_stackt::end_current_loop(
  const resolvet &resolve,
  const assign_unknownt &assign_unknown)
{
  auto &last = current_loop();
  last.end_loop(current_scope().id);
  if(getenv("LOG_LOOP") != nullptr)
  {
    std::cerr << "end loop " << current_loop().id << "\n";
  }
  current_loop().end_loop(resolve, assign_unknown);
  loop_stack.pop_back();
  push_back_scope();
}
