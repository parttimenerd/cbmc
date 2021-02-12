#include "ls_info.h"
#include <cstring>
#include <goto-programs/abstract_goto_model.h>
#include <queue>

std::ostream &operator<<(std::ostream &os, const ls_func_info &info)
{
  os << "func_info(" << info.function_id << ",";
  os << " parameters = ";
  for(auto param : info.get_parameters())
  {
    os << param << " ";
  }
  if(info.get_return())
  {
    os << " return = " << info.get_return().value() << " ";
  }
  os << " trans_assigned_globals = ";
  for(auto assigned : info.get_assigned_globals())
  {
    os << assigned << " ";
  }
  os << " trans_read_globals = ";
  for(auto read : info.get_read_globals())
  {
    os << read << " ";
  }
  return os << ")";
}

bool ls_func_info::assign_from(const ls_func_info &info)
{
  bool changed = false;
  for(auto var : info.assigned_variables)
  {
    changed |= assigned_variables.insert(var).second;
  }
  for(auto var : info.read_variables)
  {
    changed |= read_variables.insert(var).second;
  }
  return changed;
}

const dstringt_set &ls_func_info::get_assigned_variables() const
{
  return assigned_variables;
}

const dstringt_set &ls_func_info::get_directly_assigned_variables() const
{
  return assigned_variables;
}

const dstringt_set &ls_func_info::get_read_variables() const
{
  return read_variables;
}

const dstringt_set &ls_func_info::get_directly_read_variables() const
{
  return directly_read_variables;
}

bool is_global(dstringt var)
{
  return strstr(var.c_str(), "::") == nullptr &&
         !strstr(var.c_str(), "return_value");
}

dstringt_set filter_globals(const dstringt_set &vars)
{
  dstringt_set res;
  std::copy_if(
    vars.begin(), vars.end(), std::inserter(res, res.end()), is_global);
  return res;
}

dstringt_set combine(dstringt_set vars, dstringt_set &&vars2)
{
  vars.insert(vars2.begin(), vars2.end());
  return vars;
}

dstringt_set ls_func_info::get_assigned_globals() const
{
  return filter_globals(assigned_variables);
}

dstringt_set ls_func_info::get_read_globals() const
{
  return filter_globals(read_variables);
}

dstringt_set ls_func_info::get_assigned_globals_and_return() const
{
  if(return_var)
  {
    return combine(get_assigned_globals(), {return_var.value()});
  }
  return get_assigned_globals();
}

dstringt_set ls_func_info::get_parameters_and_read_globals() const
{
  return combine(get_parameters(), get_read_globals());
}

dstringt_set get_called_functions(const goto_functiont &func)
{
  dstringt_set ret;
  for(const auto &instruction : func.body.instructions)
  {
    if(instruction.type == goto_program_instruction_typet::FUNCTION_CALL)
    {
      auto &call = to_code_function_call(instruction.code);
      auto &called_func = to_symbol_expr(call.function());
      ret.emplace(called_func.get_identifier());
    }
  }
  return ret;
}

class const_expr_func_visitort : public const_expr_visitort
{
  const std::function<void(const exprt &)> func;

public:
  explicit const_expr_func_visitort(std::function<void(const exprt &)> func)
    : func(std::move(func))
  {
  }
  void operator()(const exprt &expr) override
  {
    func(expr);
  }
};

ls_infot ls_infot::create(const goto_functionst &functions)
{
  std::unordered_map<dstringt, dstringt_set> callers;
  std::unordered_map<dstringt, dstringt_set> called_map;
  for(const auto &it : functions.function_map)
  {
    callers.emplace(it.first, dstringt_set());
  }

  for(const auto &it : functions.function_map)
  {
    auto callees = get_called_functions(it.second);
    called_map.emplace(it.first, callees);
    for(const auto &called : callees)
    {
      callers[called].insert(it.first);
    }
  }

  // init the func infos
  std::unordered_map<dstringt, ls_func_info> func_infos;

  auto sub_used_func = [](const exprt &expr) {
    std::vector<dstringt> read;
    const_expr_func_visitort visitor([&](const exprt &expr) {
      if(expr.id() == ID_symbol)
      {
        read.emplace_back(to_symbol_expr(expr).get_identifier());
      }
    });
    expr.visit(visitor);
    return read;
  };

  auto used_variables = [&sub_used_func](const goto_functiont &func) {
    dstringt_set assigned;
    dstringt_set read;
    dstringt_set parameters;
    optionalt<dstringt> return_var;
    for(const auto &param : func.parameter_identifiers)
    {
      parameters.emplace(param);
    }
    auto process_right = [&read](const exprt &expr) {
      if(expr.id() == ID_symbol)
      {
        auto var = to_symbol_expr(expr).get_identifier();
        read.insert(var);
      }
    };
    const_expr_func_visitort visitor{process_right};
    for(const auto &instruction : func.body.instructions)
    {
      if(instruction.type == goto_program_instruction_typet::ASSIGN)
      {
        auto &assign = to_code_assign(instruction.code);
        if(assign.lhs().id() == ID_index)
        {
          auto &lhs = to_index_expr(assign.lhs());
          auto sub_symbols_array = sub_used_func(lhs.array());
          assigned.emplace(sub_symbols_array.front());
          read.insert(sub_symbols_array.begin() + 1, sub_symbols_array.end());
          lhs.index().visit(visitor);
        }
        else if(assign.lhs().id() == ID_symbol)
        {
          auto &lhs = to_symbol_expr(assign.lhs());
          assigned.emplace(lhs.get_identifier());
          auto str = std::string(lhs.get_identifier().c_str());
          if(
            str.size() > strlen("#return_value") &&
            str.substr(str.size() - strlen("#return_value")) == "#return_value")
          {
            return_var = lhs.get_identifier();
          }
        }
        assign.rhs().visit(visitor);
      }
      else
      {
        instruction.apply(process_right);
      }
    }
    return std::make_tuple(assigned, read, parameters, return_var);
  };

  for(const auto &it : functions.function_map)
  {
    auto vars_tuple = used_variables(it.second);
    func_infos.emplace(
      it.first,
      ls_func_info{
        it.first,
        it.second,
        callers.at(it.first),
        called_map.at(it.first),
        std::get<0>(vars_tuple),
        std::get<1>(vars_tuple),
        std::get<2>(vars_tuple),
        std::get<3>(vars_tuple)});
  }

  // now run the worklist

  dstringt_set in_queue;
  std::queue<dstringt> working_queue;

  auto push = [&](dstringt func) {
    if(in_queue.find(func) == in_queue.end())
    {
      in_queue.insert(func);
      working_queue.push(func);
    }
  };

  auto process = [&](ls_func_info &info) {
    bool changed = false;
    for(const auto &callee : info.callees)
    {
      changed |= info.assign_from(func_infos.at(callee));
    }
    return changed;
  };

  for(const auto &item : func_infos)
  {
    push(item.first);
  }

  while(!working_queue.empty())
  {
    auto &func_info = func_infos.at(working_queue.front());
    working_queue.pop();
    if(process(func_info))
    {
      for(const auto &caller : callers.at(func_info.function_id))
      {
        push(caller);
      }
    }
  }
  //std::cerr << ls_infot(func_infos) << "\n"; std::exit(1);
  return ls_infot(func_infos);
}

std::ostream &operator<<(std::ostream &os, const ls_infot &info)
{
  os << "func_info(\n";
  for(const auto &item : info.func_infos)
  {
    os << "   " << item.second << "\n";
  }
  return os << ")";
}

const ls_func_info &ls_infot::get_func_info(dstringt func_name) const
{
  return func_infos.at(func_name);
}

const dstringt_set &ls_infot::get_assigned_variables(dstringt func_name) const
{
  return get_func_info(func_name).get_assigned_variables();
}

bool ls_infot::has_func_info(dstringt func_name) const
{
  return func_infos.find(func_name) != func_infos.end();
}
