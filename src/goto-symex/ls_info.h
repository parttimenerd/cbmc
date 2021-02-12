/*******************************************************************\

Module: Basic static analysis of methods

Author: Johannes Bechberger, johannes.bechberger@kit.edu

\*******************************************************************/

/// Collected static information (prior to the symbolic execution) used by ls_stack for supporting
/// aborted loops and recursions properly

#ifndef CBMC_LS_INFO_H
#define CBMC_LS_INFO_H
#include <goto-programs/abstract_goto_model.h>

#include <ostream>
#include <utility>

using dstringt_set = std::unordered_set<dstringt>;

/// infos per functions
class ls_func_info
{
  dstringt_set assigned_variables;
  /// variables assigned in this function directly
  const dstringt_set directly_assigned_variables;

  dstringt_set read_variables;
  /// variables read in this function directly
  const dstringt_set directly_read_variables;
  const dstringt_set parameters;
  const optionalt<dstringt> return_var;

public:
  const dstringt function_id;
  const goto_functiont &function;
  const dstringt_set callers;
  const dstringt_set callees;

  ls_func_info(
    const dstringt function_id,
    const goto_functiont &function,
    dstringt_set callers,
    dstringt_set callees,
    dstringt_set directly_assigned,
    dstringt_set directly_read,
    dstringt_set parameters,
    optionalt<dstringt> return_var)
    : assigned_variables(directly_assigned),
      directly_assigned_variables(std::move(directly_assigned)),
      read_variables(directly_read),
      directly_read_variables(std::move(directly_read)),
      parameters(std::move(parameters)),
      return_var(std::move(return_var)),
      function_id(function_id),
      function(function),
      callers(std::move(callers)),
      callees(std::move(callees))
  {
  }

  /// returns true if one of the variables hasn't been recorded yet
  bool assign_from(const ls_func_info &info);

  const dstringt_set &get_assigned_variables() const;

  /// contains all variables that are assigned in the bodies of the function and its callees
  const dstringt_set &get_directly_assigned_variables() const;

  const dstringt_set &get_read_variables() const;

  /// contains all variables that are assigned in the bodies of the function and its callees
  const dstringt_set &get_directly_read_variables() const;

  dstringt_set get_assigned_globals() const;

  const optionalt<dstringt> &get_return() const
  {
    return return_var;
  }

  dstringt_set get_assigned_globals_and_return() const;

  dstringt_set get_read_globals() const;

  const dstringt_set &get_parameters() const
  {
    return parameters;
  }

  dstringt_set get_parameters_and_read_globals() const;

  friend std::ostream &operator<<(std::ostream &os, const ls_func_info &info);
};

/// precomputed information on (currently) functions
class ls_infot
{
  std::unordered_map<dstringt, ls_func_info> func_infos;

  explicit ls_infot(std::unordered_map<dstringt, ls_func_info> func_infos)
    : func_infos(std::move(func_infos))
  {
  }

public:
  const ls_func_info &get_func_info(dstringt func_name) const;

  static ls_infot create(const goto_functionst &functions);

  const dstringt_set &get_assigned_variables(dstringt func_name) const;

  friend std::ostream &operator<<(std::ostream &os, const ls_infot &info);

  bool has_func_info(dstringt func_name) const;
};

#endif //CBMC_LS_INFO_H
